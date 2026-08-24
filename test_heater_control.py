"""
test_heater_control.py
Comprehensive Unit Test Suite & Closed-Loop Thermal Simulator for Dual-Mode Heater Architecture:
Mechanical Relay Bang-Bang + DC SSR PID / Time-Proportional Control.
"""

import unittest
import math

class SimulatedThermalPlant:
    """1st order thermal bath model with ambient loss and heater power input."""
    def __init__(self, c_thermal=500.0, p_max_w=200.0, h_loss=1.5, t_ambient=25.0, initial_temp=25.0):
        self.c_thermal = c_thermal  # J / degC
        self.p_max_w = p_max_w      # W
        self.h_loss = h_loss        # W / degC
        self.t_ambient = t_ambient  # degC
        self.temp_c = initial_temp
        self.rate_c_s = 0.0

    def step(self, duty_pct, dt_s=0.10):
        p_in = (duty_pct / 100.0) * self.p_max_w
        p_loss = self.h_loss * (self.temp_c - self.t_ambient)
        p_net = p_in - p_loss
        self.rate_c_s = p_net / self.c_thermal
        self.temp_c += self.rate_c_s * dt_s
        return self.temp_c, self.rate_c_s


class SimulatedHeaterPID:
    def __init__(self, kp=10.0, ki=0.20, kd=15.0, i_min=0.0, i_max=100.0, window_ms=2000, min_on_ms=50, min_off_ms=50):
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.i_min = i_min
        self.i_max = i_max
        self.window_ms = window_ms
        self.min_on_ms = min_on_ms
        self.min_off_ms = min_off_ms
        
        self.integral = 0.0
        self.prev_error = 0.0
        self.p_term = 0.0
        self.i_term = 0.0
        self.d_term = 0.0
        self.output_pct = 0.0
        
    def reset(self):
        self.integral = 0.0
        self.prev_error = 0.0
        self.p_term = 0.0
        self.i_term = 0.0
        self.d_term = 0.0
        self.output_pct = 0.0

    def compute(self, setpoint_c, temp_c, temp_rate_c_per_s, dt_s=0.10):
        error = setpoint_c - temp_c
        self.prev_error = error
        self.p_term = self.kp * error
        
        next_integral = self.integral + (self.ki * error * dt_s)
        if next_integral < self.i_min:
            next_integral = self.i_min
        elif next_integral > self.i_max:
            next_integral = self.i_max
            
        if (self.output_pct < 100.0 or error < 0.0) and (self.output_pct > 0.0 or error > 0.0):
            self.integral = next_integral
        self.i_term = self.integral
        
        self.d_term = -self.kd * temp_rate_c_per_s
        raw_output = self.p_term + self.i_term + self.d_term
        
        if raw_output < 0.0:
            self.output_pct = 0.0
        elif raw_output > 100.0:
            self.output_pct = 100.0
        else:
            self.output_pct = raw_output
        return self.output_pct

    def get_time_proportional_output(self, window_elapsed_ms):
        on_time_ms = int((self.output_pct / 100.0) * self.window_ms)
        if on_time_ms < self.min_on_ms:
            on_time_ms = 0
        elif (self.window_ms - on_time_ms) < self.min_off_ms:
            on_time_ms = self.window_ms
        return 1 if (window_elapsed_ms < on_time_ms) else 0


class SimulatedRelayController:
    def __init__(self, hysteresis_c=1.0, min_on_ms=10000, min_off_ms=10000):
        self.hysteresis_c = hysteresis_c
        self.min_on_ms = min_on_ms
        self.min_off_ms = min_off_ms
        self.state = 0
        self.last_switch_ms = -min_off_ms

    def reset(self):
        self.state = 0
        self.last_switch_ms = -self.min_off_ms

    def compute(self, setpoint_c, temp_c, current_time_ms):
        elapsed = current_time_ms - self.last_switch_ms
        if temp_c <= (setpoint_c - self.hysteresis_c):
            if self.state == 0 and elapsed >= self.min_off_ms:
                self.state = 1
                self.last_switch_ms = current_time_ms
        elif temp_c >= (setpoint_c + self.hysteresis_c):
            if self.state != 0 and elapsed >= self.min_on_ms:
                self.state = 0
                self.last_switch_ms = current_time_ms
        return self.state


class TestDualModeHeaterControl(unittest.TestCase):
    def setUp(self):
        self.pid = SimulatedHeaterPID()
        self.relay = SimulatedRelayController()

    def test_pid_zero_error(self):
        """At setpoint with zero rate and zero integral, output is 0.0%."""
        duty = self.pid.compute(setpoint_c=57.0, temp_c=57.0, temp_rate_c_per_s=0.0)
        self.assertEqual(duty, 0.0)
        self.assertEqual(self.pid.p_term, 0.0)
        self.assertEqual(self.pid.d_term, 0.0)

    def test_pid_positive_error_heating(self):
        """When temp is below setpoint (50C vs 57C), P-term drives high output."""
        duty = self.pid.compute(setpoint_c=57.0, temp_c=50.0, temp_rate_c_per_s=0.0, dt_s=0.1)
        self.assertAlmostEqual(self.pid.p_term, 70.0, places=2)
        self.assertAlmostEqual(self.pid.i_term, 0.14, places=2)
        self.assertAlmostEqual(duty, 70.14, places=2)

    def test_pid_negative_error_cooling(self):
        """When temp is above setpoint (60C vs 57C), output saturates to 0.0%."""
        duty = self.pid.compute(setpoint_c=57.0, temp_c=60.0, temp_rate_c_per_s=0.0, dt_s=0.1)
        self.assertAlmostEqual(self.pid.p_term, -30.0, places=2)
        self.assertAlmostEqual(duty, 0.0, places=2)

    def test_pid_anti_windup_clamp(self):
        """Integral cannot exceed I_MAX (100.0) even after prolonged under-temperature."""
        for _ in range(200):
            self.pid.compute(setpoint_c=57.0, temp_c=30.0, temp_rate_c_per_s=0.0, dt_s=0.1)
        self.assertLessEqual(self.pid.integral, 100.0)
        self.assertGreaterEqual(self.pid.integral, 0.0)

    def test_pid_filtered_derivative_suppression(self):
        """Rapidly rising temp produces negative D-term, reducing duty before overshoot."""
        duty_still = self.pid.compute(setpoint_c=57.0, temp_c=56.0, temp_rate_c_per_s=0.0, dt_s=0.1)
        self.pid.reset()
        duty_rising = self.pid.compute(setpoint_c=57.0, temp_c=56.0, temp_rate_c_per_s=0.10, dt_s=0.1)
        self.assertAlmostEqual(duty_still, 10.02, places=2)
        self.assertAlmostEqual(duty_rising, 8.52, places=2)
        self.assertLess(duty_rising, duty_still)

    def test_output_saturation_bounds(self):
        """Duty cycle is strictly bounded within [0.0, 100.0] %."""
        duty_max = self.pid.compute(setpoint_c=57.0, temp_c=0.0, temp_rate_c_per_s=0.0, dt_s=0.1)
        self.assertAlmostEqual(duty_max, 100.0, places=2)
        duty_min = self.pid.compute(setpoint_c=57.0, temp_c=100.0, temp_rate_c_per_s=0.0, dt_s=0.1)
        self.assertAlmostEqual(duty_min, 0.0, places=2)

    def test_stop_reset_behavior(self):
        """STOP immediately zeros duty, integral, and terms."""
        self.pid.compute(setpoint_c=57.0, temp_c=40.0, temp_rate_c_per_s=0.0, dt_s=0.1)
        self.assertGreater(self.pid.output_pct, 0.0)
        self.pid.reset()
        self.assertAlmostEqual(self.pid.output_pct, 0.0, places=2)
        self.assertAlmostEqual(self.pid.integral, 0.0, places=2)

    def test_time_proportional_window_mapping(self):
        """50% duty in 2000ms window yields ON for first 1000ms and OFF for last 1000ms."""
        self.pid.output_pct = 50.0
        self.assertEqual(self.pid.get_time_proportional_output(window_elapsed_ms=500), 1)
        self.assertEqual(self.pid.get_time_proportional_output(window_elapsed_ms=999), 1)
        self.assertEqual(self.pid.get_time_proportional_output(window_elapsed_ms=1000), 0)
        self.assertEqual(self.pid.get_time_proportional_output(window_elapsed_ms=1900), 0)

    def test_ssr_min_pulse_protection(self):
        """Duty below 2.5% (50ms) suppresses pulse to 0; above 97.5% locks to full 100%."""
        self.pid.output_pct = 1.0
        self.assertEqual(self.pid.get_time_proportional_output(window_elapsed_ms=10), 0)
        
        self.pid.output_pct = 99.0
        self.assertEqual(self.pid.get_time_proportional_output(window_elapsed_ms=1970), 1)
        self.assertEqual(self.pid.get_time_proportional_output(window_elapsed_ms=1990), 1)

    # =========================================================================
    # SECTION 7: DETAILED CLOSED-LOOP SIMULATION SUITE (SCENARIOS A -> O)
    # =========================================================================

    def run_closed_loop_sim(self, initial_temp=50.0, target_temp=57.0, duration_s=300.0, noise_amp=0.0):
        """Simulates closed loop heating and returns performance metrics."""
        plant = SimulatedThermalPlant(initial_temp=initial_temp)
        pid = SimulatedHeaterPID()
        dt_s = 0.10
        steps = int(duration_s / dt_s)
        
        temps = []
        duties = []
        
        for step_i in range(steps):
            t_meas = plant.temp_c + (noise_amp * math.sin(step_i * 0.5))
            duty = pid.compute(setpoint_c=target_temp, temp_c=t_meas, temp_rate_c_per_s=plant.rate_c_s, dt_s=dt_s)
            plant.step(duty_pct=duty, dt_s=dt_s)
            temps.append(plant.temp_c)
            duties.append(duty)
            
        peak_overshoot = max(0.0, max(temps) - target_temp)
        steady_state_slice = temps[int(steps * 0.8):]
        steady_state_error = abs((sum(steady_state_slice) / len(steady_state_slice)) - target_temp)
        
        return {
            'final_temp': temps[-1],
            'peak_overshoot': peak_overshoot,
            'steady_state_error': steady_state_error,
            'max_duty': max(duties),
            'min_duty': min(duties)
        }

    def test_scenario_A_50_to_57(self):
        """Scenario A: 50C -> 57C closed-loop heating."""
        res = self.run_closed_loop_sim(initial_temp=50.0, target_temp=57.0, duration_s=300.0)
        self.assertLess(res['peak_overshoot'], 1.5)
        self.assertLess(res['steady_state_error'], 0.2)

    def test_scenario_B_40_to_57(self):
        """Scenario B: 40C -> 57C larger step heating."""
        res = self.run_closed_loop_sim(initial_temp=40.0, target_temp=57.0, duration_s=350.0)
        self.assertLess(res['peak_overshoot'], 1.8)
        self.assertLess(res['steady_state_error'], 0.2)

    def test_scenario_C_60_to_57(self):
        """Scenario C: 60C -> 57C cooldown from over-temperature."""
        res = self.run_closed_loop_sim(initial_temp=60.0, target_temp=57.0, duration_s=150.0)
        self.assertEqual(res['min_duty'], 0.0)

    def test_scenario_D_noise_0_1(self):
        """Scenario D: 57C with +-0.1C sensor noise."""
        res = self.run_closed_loop_sim(initial_temp=56.0, target_temp=57.0, duration_s=300.0, noise_amp=0.10)
        self.assertLess(res['steady_state_error'], 0.25)

    def test_scenario_E_noise_0_2(self):
        """Scenario E: 57C with +-0.2C sensor noise."""
        res = self.run_closed_loop_sim(initial_temp=56.0, target_temp=57.0, duration_s=300.0, noise_amp=0.20)
        self.assertLess(res['steady_state_error'], 0.35)


    def test_scenario_F_high_initial_thermal_slope(self):
        """Scenario F: High initial rate dampens duty via D-term."""
        plant = SimulatedThermalPlant(initial_temp=55.0)
        plant.rate_c_s = 0.50  # High positive rate
        duty = self.pid.compute(setpoint_c=57.0, temp_c=55.0, temp_rate_c_per_s=plant.rate_c_s, dt_s=0.1)
        self.assertLess(duty, 20.0)  # P=20, D=-7.5 -> duty throttled

    def test_scenario_G_setpoint_change_50_to_57(self):
        """Scenario G: Runtime setpoint change 50C -> 57C."""
        duty_50 = self.pid.compute(setpoint_c=50.0, temp_c=50.0, temp_rate_c_per_s=0.0)
        self.assertEqual(duty_50, 0.0)
        duty_57 = self.pid.compute(setpoint_c=57.0, temp_c=50.0, temp_rate_c_per_s=0.0)
        self.assertGreater(duty_57, 50.0)

    def test_scenario_H_setpoint_change_57_to_60(self):
        """Scenario H: Runtime setpoint change 57C -> 60C."""
        duty_57 = self.pid.compute(setpoint_c=57.0, temp_c=57.0, temp_rate_c_per_s=0.0)
        self.assertEqual(duty_57, 0.0)
        duty_60 = self.pid.compute(setpoint_c=60.0, temp_c=57.0, temp_rate_c_per_s=0.0)
        self.assertAlmostEqual(duty_60, 30.06, places=1)

    def test_scenario_I_heater_saturation(self):
        """Scenario I: Massive error clamps output to 100%."""
        duty = self.pid.compute(setpoint_c=90.0, temp_c=25.0, temp_rate_c_per_s=0.0)
        self.assertEqual(duty, 100.0)

    def test_scenario_J_cooling_without_heater(self):
        """Scenario J: When target < ambient, duty remains strictly 0%."""
        duty = self.pid.compute(setpoint_c=20.0, temp_c=25.0, temp_rate_c_per_s=0.0)
        self.assertEqual(duty, 0.0)

    def test_scenario_K_pt100_invalid(self):
        """Scenario K: Invalid sensor state forces reset."""
        self.pid.compute(setpoint_c=57.0, temp_c=30.0, temp_rate_c_per_s=0.0)
        self.pid.reset()  # Simulates sensor fault handler
        self.assertEqual(self.pid.output_pct, 0.0)

    def test_scenario_L_M_stop_and_fault(self):
        """Scenario L & M: STOP and FAULT unconditionally zero output."""
        self.pid.output_pct = 85.0
        self.pid.integral = 40.0
        self.pid.reset()
        self.assertEqual(self.pid.output_pct, 0.0)
        self.assertEqual(self.pid.integral, 0.0)

    def test_scenario_N_O_bumpless_relay_ssr_transitions(self):
        """Scenario N & O: Switching between Relay and SSR resets previous state cleanly."""
        # 1. Running in Relay mode
        relay_state = self.relay.compute(setpoint_c=57.0, temp_c=55.0, current_time_ms=1000)
        self.assertEqual(relay_state, 1)
        
        # 2. Switch to SSR: relay forced off, PID initialized
        self.relay.reset()
        self.pid.reset()
        self.assertEqual(self.relay.state, 0)
        ssr_duty = self.pid.compute(setpoint_c=57.0, temp_c=55.0, temp_rate_c_per_s=0.0)
        self.assertGreater(ssr_duty, 0.0)
        
        # 3. Switch back to Relay: PID reset, relay starts clean
        self.pid.reset()
        self.assertEqual(self.pid.output_pct, 0.0)
        relay_state = self.relay.compute(setpoint_c=57.0, temp_c=55.0, current_time_ms=25000)
        self.assertEqual(relay_state, 1)


if __name__ == '__main__':
    unittest.main()
