#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "column.h"
#include "datatable.h"
#include "myassert.h"
#include "rowindex.h"
#include "utils.h"



/**
 * Append to datatable `dt` a list of datatables `dts`. There are `ndts` items
 * in this list. The `cols` array of dimensions `ncols x ndts` specifies how
 * the columns should be matched.
 * In particular, the datatable `dt` will be expanded to have `ncols` columns,
 * and `dt->nrows + sum(dti->nrows for dti in dts)` rows. The `i`th column
 * in the expanded datatable will have the following structure: first comes the
 * data from `i`th column of `dt` (if `i < dt->ncols`, otherwise NAs); after
 * that come `ndts` blocks of rows, each `j`th block having data from column
 * number `cols[i][j]` in datatable `dts[j]` (if `cols[i][j] >= 0`, otherwise
 * NAs).
 */
DataTable*
DataTable::rbind(DataTable **dts, int **cols, int ndts, int64_t ncols__)
{
    assert(ncols__ >= ncols);

    // If dt is a view datatable, then it must be converted to MT_DATA
    reify();

    dtrealloc(columns, Column*, ncols__ + 1);
    for (int64_t i = ncols; i < ncols__; ++i) {
        columns[i] = NULL;
    }
    columns[ncols__] = NULL;

    int64_t nrows__ = nrows;
    for (int i = 0; i < ndts; ++i) {
        nrows__ += dts[i]->nrows;
    }

    Column **cols0 = NULL;
    dtmalloc(cols0, Column*, ndts + 1);
    cols0[ndts] = NULL;

    for (int64_t i = 0; i < ncols__; ++i) {
        Column *ret = NULL;
        Column *col0 = (i < ncols)
            ? columns[i]
            : new Column(ST_VOID, (size_t) nrows);
        for (int j = 0; j < ndts; ++j) {
            if (cols[i][j] < 0) {
                cols0[j] = new Column(ST_VOID, (size_t) dts[j]->nrows);
            } else if (dts[j]->rowindex) {
                cols0[j] = dts[j]->columns[cols[i][j]]->extract(dts[j]->rowindex);
            } else {
                cols0[j] = new Column(dts[j]->columns[cols[i][j]]);
            }
            if (cols0[j] == NULL) return NULL;
        }
        ret = col0->rbind(cols0);
        if (ret == NULL) return NULL;
        columns[i] = ret;
    }
    ncols = ncols__;
    nrows = nrows__;
    return this;
}