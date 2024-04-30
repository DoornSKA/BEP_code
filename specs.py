# TODO: improve naming conventions
# Ik ben er ook!
# Verandering2

from cmath import pi, sqrt

class Specs:
    def __init__(self):
        print("---- LC oscilator")
        self.C_pulse = 185e-6
        print(f"-- C_pulse = {self.C_pulse}")
        self.L_pulse = 16e-6
        print(f"-- L_pulse = {self.L_pulse}")
        self.R_coil = 50e-3
        print(f"-- R_coil = {self.R_coil}")
        print("---- non-idealities")
        self.R_d_pulse = self.R_coil/100
        print(f"-- R_d_pulse = {self.R_d_pulse}")
        self.R_c_pulse = 1.5e-3
        print(f"-- R_c_pulse = {self.R_c_pulse}")
        self.R_igbt_pulse = self.R_coil/100
        print(f"-- R_igbt_pulse = {self.R_igbt_pulse}")
        self.R_cable_pulse = 0
        print(f"-- R_cable_pulse = {self.R_cable_pulse}")
        self.L_c_pulse = 5e-9
        print(f"-- L_c_pulse = {self.L_c_pulse}")
        self.L_igbt = 150e-9
        print(f"-- L_igbt = {self.L_igbt}")
        self.C_coil = 6e-6
        print(f"-- C_coil = {self.C_coil}")

        print("")
        self.C_smoothing = 1e-3
        print(f"-- C_smoothing = {self.C_smoothing}")
        self.IGBT_charge = 0
        print(f"-- IGBT_charge = {self.IGBT_charge}")
        self.R_charge = 100
        print(f"-- R_charge = {self.R_charge}")
        self.R_nonideal_charge = 0
        print(f"-- R_nonideal_charge = {self.R_nonideal_charge}")
        print("")
    
        self.damping_factor = 0
        self.resonant_frequency = 0
        self.rc5 = 0
        self.period = 0
        self.damping_ratio = 0
        self.inductor_impedance = 0
        self.update()

    def update(self):
        R_LC_total = self.R_coil + self.R_igbt_pulse + self.R_c_pulse
        L_LC_total = self.L_c_pulse + self.L_pulse

        self.damping_factor = R_LC_total/(2*L_LC_total)
        self.resonant_frequency = 1/sqrt(self.C_pulse*L_LC_total)
        self.damping_ratio = self.damping_factor/self.resonant_frequency
        self.rc5 = R_LC_total+self.C_pulse
        self.period = 2*pi/self.resonant_frequency
        
        Z_l_coil = 1j*self.resonant_frequency*self.L_pulse
        Z_c_coil = 1/(1j*self.resonant_frequency*self.L_pulse)

        self.inductor_impedance = ((self.R_coil + Z_l_coil) * Z_c_coil)/(self.R_coil + Z_l_coil + Z_c_coil)

        self.print_calculated()

    def print_calculated(self):
        print("---- Calculated values")
        print(f"-- damping_factor = {self.damping_factor}")
        print(f"-- resonant_frequency = {self.resonant_frequency}")
        print(f"-- damping_ratio = {self.damping_ratio}")
        print(f"-- 5RC = {self.rc5}")
        print(f"-- period = {self.period}")
        print(f"-- inductor_impedance = {self.inductor_impedance}")
