//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_COLUMN_CUT_h
#define dt_COLUMN_CUT_h
#include <memory>
#include "column/const.h"
#include "column/virtual.h"
#include "models/utils.h"
#include "stype.h"


namespace dt {

/**
 *  Virtual column to bin numeric values into discrete intervals.
 *
 *  The binning method consists of the following steps
 *
 *    1) calculate min/max for the input column, and if one of these is NaN
 *       or inf, or `nbins == 0` return the `ConstNa_ColumnImpl`.
 *
 *    2) for valid and finite min/max normalize column data to
 *
 *         - [0; 1 - epsilon] interval, if right-closed bins are requested, or to
 *         - [epsilon - 1; 0] interval, otherwise.
 *
 *       Then, multiply the normalized values by the number of the requested
 *       bins and add a proper shift to compute the final bin ids. The following
 *       formula is used for that (note, casting to an integer will truncate
 *       toward zero)
 *
 *         bin_id_i = static_cast<int32_t>(x_i * a + b) + shift
 *
 *       2.1) if `max == min`, all values end up in the central bin which id
 *            is determined based on the `right_closed` parameter being
 *            `true` or `false`, i.e.
 *
 *              a = 0;
 *              b = nbins * (1 ∓ epsilon) / 2;
 *              shift = 0
 *
 *       2.2) when `min != max`, and `right_closed == true`, set
 *
 *              a = (1 - epsilon) * nbins / (max - min)
 *              b = -a * min
 *              shift = 0
 *
 *            scaling data to [0; 1 - epsilon] and then multiplying them
 *            by `nbins`.
 *
 *       2.3) When `min != max`, and `right_closed == false`, set
 *
 *              a = (1 - epsilon) * nbins / (max - min)
 *              b = -a * min + (epsilon - 1) * nbins
 *              shift = nbins - 1;
 *
 *            scaling data to [epsilon - 1; 0], multiplying them by `nbins`,
 *            and then shifting the resulting values by `nbins - 1`. The final
 *            shift converts the auxiliary negative bin ids to the corresponding
 *            positive bin ids.
 *
 */
class Cut_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column col_;
    double a_, b_;
    int32_t shift_;
    size_t: 32;

  public:

    template <typename T>
    static ColumnImpl* make(Column&& col, size_t nbins, bool right_closed) {
      T tmin, tmax;
      bool min_valid = col.stats()->get_stat(Stat::Min, &tmin);
      bool max_valid = col.stats()->get_stat(Stat::Max, &tmax);
      if (!min_valid || !max_valid || _isinf(tmin) || _isinf(tmax) || nbins == 0) {
        return new ConstNa_ColumnImpl(col.nrows(), dt::SType::INT32);
      } else {
        double min = static_cast<double>(tmin);
        double max = static_cast<double>(tmax);
        double a, b;
        int32_t shift;

        col.cast_inplace(SType::FLOAT64);
        set_cut_coeffs(a, b, shift, min, max, nbins, right_closed);

        return new Cut_ColumnImpl(std::move(col), a, b, shift);
      }

    }

    Cut_ColumnImpl(Column&& col, double a, double b, int32_t shift)
      : Virtual_ColumnImpl(col.nrows(), dt::SType::INT32),
        col_(col),
        a_(a),
        b_(b),
        shift_(shift) {}

    ColumnImpl* clone() const override {
      return new Cut_ColumnImpl(Column(col_), a_, b_, shift_);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return col_;
    }


    bool get_element(size_t i, int32_t* out)  const override {
      double value;
      bool is_valid = col_.get_element(i, &value);
      *out = static_cast<int32_t>(a_ * value + b_) + shift_;
      return is_valid;
    }


    static void set_cut_coeffs(double& a, double& b, int32_t& shift,
                               const double min, const double max,
                               const size_t nbins, const bool right_closed)
    {
      const double epsilon = static_cast<double>(
                               std::numeric_limits<float>::epsilon()
                             );
      shift = 0;

      if (min == max) {
        a = 0;
        b = 0.5 * nbins * (1 + (1 - 2 * right_closed) * epsilon);
      } else {
        a = (1 - epsilon) * nbins / (max - min);
        b = -a * min;
        if (!right_closed) {
          b += (epsilon - 1) * nbins;
          shift = static_cast<int32_t>(nbins) - 1;
        }
      }
    }

};



}  // namespace dt


#endif