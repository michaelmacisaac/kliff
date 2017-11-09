# Test the symmetry functions values. The zeta_ref are taken from aenet by
# Nongnuch Artrith.

from __future__ import print_function
from openkim_fit.descriptor import Descriptor
from openkim_fit.dataset import DataSet
from collections import OrderedDict
import numpy as np
import pytest

zeta_ref =np.array([
           [  8.26552746e-01,   8.21898457e-01,   7.76968604e-01,   8.23872603e-01,
              7.82434629e-01,   1.09924878e-02,   8.97010397e-02,   3.12057742e-02,
              2.19188995e-01 ],
           [  1.97203988e+00,   1.96235440e+00,   1.86742677e+00,   1.96646887e+00,
              1.87994799e+00,   1.33160610e-01,   3.59292937e-02,   1.94138336e+00,
              3.60417941e-01 ],
           [  1.12436604e+00,   1.11713094e+00,   1.04704155e+00,   1.12020088e+00,
              1.05573420e+00,   1.44717148e-02,   6.77068300e-02,   4.65310973e-01,
              2.10094069e-01 ],
           [  6.14948682e-01,   6.10954381e-01,   5.72406435e-01,   6.12648545e-01,
              5.77088606e-01,  7.40196968e-04,   6.38212636e-03,   5.59365334e-02,
              4.53723038e-02 ],
           [  1.14547465e+00,   1.13801776e+00,   1.06581102e+00,   1.14118167e+00,
              1.07474500e+00,   1.55422421e-02,   8.48799596e-02,   4.80992881e-01,
              2.40807811e-01 ],
           [  5.85871635e-01,   5.81613611e-01,   5.40597021e-01,   5.83419313e-01,
              5.45526214e-01,   6.59891283e-04,   5.86380395e-03,   6.00226823e-02,
              4.85641661e-02 ],
           [  1.04728862e+00,   1.04008269e+00,   9.70373031e-01,   1.04313984e+00,
              9.78952121e-01,   1.14097233e-02,   6.60188471e-02,   3.84706149e-01,
              2.02570556e-01 ],
           [  5.57949531e-01,   5.53995531e-01,   5.15882607e-01,   5.55672409e-01,
              5.20480097e-01,   5.08434077e-04,   3.87820957e-03,   5.09993748e-02,
              3.56988064e-02]])



def get_descriptor():

  cutfunc = 'cos'
  cutvalue = {'Si-Si': 4.5}
  desc_params = OrderedDict()

  desc_params['g1'] = None

  desc_params['g2'] = [
    {'eta':0.0009, 'Rs':0.},
    {'eta':0.01,   'Rs':0.},
  ]

  desc_params['g3'] = [
    {'kappa':0.03214},
    {'kappa':0.13123}
  ]

  desc_params['g4'] = [
    {'zeta':1 ,  'lambda':-1, 'eta':0.0001},
    {'zeta':2 ,  'lambda':1, 'eta':0.003 }
  ]

  desc_params['g5'] = [
    {'zeta':1 ,  'lambda':-1, 'eta':0.0001},
    {'zeta':2 ,  'lambda':1, 'eta':0.003 }
  ]

  # tranfer units from bohr to angstrom
#  bhor2ang = 0.529177
#
#  for key, values in desc_params.iteritems():
#    for val in values:
#      if key == 'g2' or key =='g4' or key == 'g5':
#        val['eta'] /= bhor2ang**2       # note eta is with units of Length^-2
#      elif key == 'g3':
#        val['kappa'] /= bhor2ang

  print ('# Descriptor  size')
  total = 0
  for key, values in desc_params.iteritems():
    if values is not None:
      size = len(values)
    else:
      size = 1
    print(key,' '*10, size)
    total += size
  print('Total number of descriptors:', total)

  # create all descriptors
  desc = Descriptor(cutfunc, cutvalue, desc_params, fit_forces=False)

  return desc


def test_desc():

  tset = DataSet()
  tset.read('./training_set/training_set_Si.xyz')
  configs = tset.get_configs()

  desc = get_descriptor()
  zeta, _ = desc.generate_generalized_coords(configs[0])

  tol = 1e-6
  assert np.array(zeta) == pytest.approx(zeta_ref, tol)

  print('generalized coords')
  print(zeta)


if __name__ == '__main__':
  test_desc()

