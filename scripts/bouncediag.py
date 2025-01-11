#!/usr/bin/python3

import math, os, sys

def printf(format, *args):
    sys.stdout.write(format % args)

def gamma(r, z):
    """Returns the reflections ratio.

    @param r the source or load impedance
    @param z the characteristic impedance of the transmission line
    @return \Gamma_L.
    """
    if (r == math.inf):
        return 1
    return (r - z) / (r + z)


class Params:
    def __init__(self, z_0):
        """ @param z_0 the characteristic impedance of the transmission line"""
        self._z_0 = z_0

        
    def voltage_source(self, voltage, r_source):
        """
        @param voltage the source voltage
        @param r_source the voltage source impedance
        """
        self._v_0 = voltage
        self._z_s = r_source
        self._gamma_s = gamma(r = self._z_s, z = self._z_0)
        self._v_1_plus = self._v_0 * self._z_0 / (self._z_0 + self._z_s)
        return self

    def load(self, r_load):
        self._z_l = r_load
        self._gamma_l = gamma(r = self._z_l, z = self._z_0)
        return self


def printbouncediag(params):
    v_curr = params._v_1_plus
    v_last = v_curr
    for i in range(0, 40, 2):
        printf("V(s,%d)= %.3f V  V^%d+ = %.3f V\n", i, v_curr, i, v_last)
        i_source = (params._v_0 - v_curr) * 1000 / params._z_s
        printf("I(s,%d)= %.2f mA\n", i, i_source)
        
        v_minus = v_last * params._gamma_l
        v_curr += v_minus
        v_last = v_minus
        i_target_ma = v_curr * 1000 / params._z_l
        printf("                                ")
        printf("V(t,%d)= %.3f V ", i+1, v_curr)
        printf("V^%d- = %.3f V\n", i+1, v_last)
        printf("                                ")
        printf("I(t,%d)= %.2f mA\n", i+1, i_target_ma)
        v_plus = v_last * params._gamma_s
        v_curr += v_plus
        v_last = v_plus
        
        
        


p = Params(z_0=152).voltage_source(voltage=2.1, r_source=22).load(4.8)
printbouncediag(p)
