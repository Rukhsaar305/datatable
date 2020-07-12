#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
from datatable import dt, stype, f, cut
from tests import assert_equals
import pytest
import random
import math


#-------------------------------------------------------------------------------
# cut()
#-------------------------------------------------------------------------------

def test_cut_error_noargs():
    msg = r"Function cut\(\) requires one positional argument, but none were given"
    with pytest.raises(TypeError, match=msg):
        cut()


def test_cut_error_wrong_column_types():
    DT = dt.Frame([[1, 0], ["1", "0"]])
    msg = r"cut\(\) can only be applied to numeric columns, instead column 1 has an stype: str32"
    with pytest.raises(TypeError, match=msg):
        cut(DT)


def test_cut_error_negative_bins():
    msg = "Integer value cannot be negative"
    DT = dt.Frame(range(10))
    with pytest.raises(ValueError, match=msg):
        cut(DT, -10)


def test_cut_error_negative_bins_list():
    msg = "Integer value cannot be negative"
    DT = dt.Frame([[3, 1, 4], [1, 5, 9]])
    with pytest.raises(ValueError, match=msg):
        cut(DT, [10, -1])


def test_cut_error_inconsistent_bins():
    msg = ("When bins is a list or a tuple, its length must be the same as "
    	   "the number of columns in the frame/expression, i.e. 2, instead got: 1")
    DT = dt.Frame([[3, 1, 4], [1, 5, 9]])
    with pytest.raises(ValueError, match=msg):
        cut(DT, [10])


def test_cut_error_groupby():
    msg = r"cut\(\) cannot be used in a groupby context"
    DT = dt.Frame(range(10))
    with pytest.raises(NotImplementedError, match=msg):
        DT[:, cut(f[0]), f[0]]


def test_cut_empty_frame():
	DT_cut = cut(dt.Frame())
	assert_equals(DT_cut, dt.Frame())


def test_cut_zero_bins():
	DT_cut = cut(dt.Frame(range(10)), 0)
	assert_equals(DT_cut, dt.Frame([None] * 10, stypes = [stype.int32]))


def test_cut_trivial():
	DT = dt.Frame({"trivial": range(10)})
	DT_cut = cut(DT)
	assert_equals(DT, DT_cut)


def test_cut_expr():
	DT = dt.Frame([range(0, 30, 3), range(0, 20, 2)])
	DT_cut = DT[:, cut(cut(f[0] - f[1]))]
	assert_equals(dt.Frame(range(10)), DT_cut)


def test_cut_one_row():
	DT = dt.Frame([[True], [404], [3.1415926], [None]])
	DT_cut = cut(DT)
	assert(DT_cut.to_list() == [[4], [4], [4], [None]])


def test_cut_simple():
	DT = dt.Frame(
	       [[True, None, False, False, True, None],
	       [3, None, 4, 1, 5, 4],
	       [None, 1.4, 4.1, 1.5, 5.9, 1.4],
	       [math.inf, 1.4, 4.1, 1.5, 5.9, 1.4],
	       [-math.inf, 1.4, 4.1, 1.5, 5.9, 1.4]],
	       names = ["bool", "int", "float", "inf_max", "inf_min"]
	     )
	DT_ref = dt.Frame(
		       [[1, None, 0, 0, 1, None],
		       [1, None, 2, 0, 2, 2],
		       [None, 0, 5, 0, 9, 0],
		       [None] * 6,
		       [None] * 6],
		       names = ["bool", "int", "float", "inf_max", "inf_min"],
		       stypes = [stype.int32] * 5
             )

	DT_cut_list = cut(DT, bins = [2, 3, 10, 3, 2])
	DT_cut_tuple = cut(DT, bins = (2, 3, 10, 3, 2))
	assert_equals(DT_ref, DT_cut_list)
	assert_equals(DT_ref, DT_cut_tuple)



#-------------------------------------------------------------------------------
# This test may fail in rare situations due to Pandas inconsistency,
# see `test_cut_pandas_inconsistency()`
#-------------------------------------------------------------------------------
@pytest.mark.parametrize("seed", [random.getrandbits(32) for _ in range(10)])
def test_cut_random(pandas, seed):
	random.seed(seed)
	max_size = 100
	max_value = 1000

	n = random.randint(1, max_size)

	bins = [random.randint(1, max_size) for _ in range(3)]
	data = [[] for _ in range(3)]

	for _ in range(n):
		data[0].append(random.randint(0, 1))
		data[1].append(random.randint(-max_value, max_value))
		data[2].append(random.random() * 2 * max_value - max_value)

	DT = dt.Frame(data, stypes = [stype.bool8, stype.int32, stype.float64])
	DT_cut = cut(DT, bins)
	PD = [pandas.cut(data[i], bins[i], labels=False) for i in range(3)]

	assert([list(PD[i]) for i in range(3)] == DT_cut.to_list())


#-------------------------------------------------------------------------------
# pandas.cut() behaves inconsistently in this test, i.e.
#
#   pandas.cut(data, nbins, labels = False)
#
# results in `[0 21 41]` bins, while it should be `[0 20 41]`.
#
# See the following issue for more details
# https://github.com/pandas-dev/pandas/issues/35126
#-------------------------------------------------------------------------------
def test_cut_pandas_inconsistency(pandas):
	nbins = 42
	data = [-97, 0, 97]
	DT = dt.Frame(data)
	DT_cut = cut(DT, nbins)
	PD = pandas.cut(data, nbins, labels = False)
	assert(DT_cut.to_list() == [[0, 20, 41]])

	# Testing that Pandas results are inconsistent
	assert(list(PD) == [0, 21, 41])