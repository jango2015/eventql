/**
 * Copyright (c) 2016 DeepCortex GmbH <legal@eventql.io>
 * Authors:
 *   - Paul Asmuth <paul@eventql.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#pragma once
#include <stdlib.h>
#include <assert.h>
#include <unordered_map>
#include <eventql/util/exception.h>
#include <eventql/util/charts/canvas.h>
#include <eventql/util/charts/barchart.h>
#include <eventql/util/charts/series.h>
#include <eventql/sql/qtree/DrawStatementNode.h>
#include <eventql/sql/runtime/compiler.h>
#include <eventql/sql/runtime/vm.h>
#include <eventql/sql/extensions/chartsql/seriesadapter.h>

namespace csql {
class DrawStatement;

class ChartBuilder {
public:

  ChartBuilder(
      util::chart::Canvas* canvas,
      RefPtr<DrawStatementNode> draw_stmt) :
      canvas_(canvas),
      draw_stmt_(draw_stmt) {}

  void executeStatement(
      Transaction* txn,
      RefPtr<TableExpressionNode> stmt,
      ScopedPtr<ResultCursor> result_cursor) {
    name_ind_ = stmt->getComputedColumnIndex("series");

    x_ind_ = stmt->getComputedColumnIndex("x");
    if (x_ind_ < 0) {
      x_ind_ = stmt->getComputedColumnIndex("X");
    }

    y_ind_ = stmt->getComputedColumnIndex("y");
    if (y_ind_ < 0) {
      y_ind_ = stmt->getComputedColumnIndex("Y");
    }

    z_ind_ = stmt->getComputedColumnIndex("z");
    if (z_ind_ < 0) {
      z_ind_ = stmt->getComputedColumnIndex("Z");
    }

    prop_indexes_.clear();

    int color_ind = stmt->getComputedColumnIndex("color");
    if (color_ind >= 0) {
      prop_indexes_.emplace_back(util::chart::Series::P_COLOR, color_ind);
    }

    int label_ind = stmt->getComputedColumnIndex("label");
    if (label_ind >= 0) {
      prop_indexes_.emplace_back(util::chart::Series::P_LABEL, label_ind);
    }

    int line_style_ind = stmt->getComputedColumnIndex("linestyle");
    if (line_style_ind >= 0) {
      prop_indexes_.emplace_back(
          util::chart::Series::P_LINE_STYLE,
          line_style_ind);
    }

    int line_width_ind = stmt->getComputedColumnIndex("linewidth");
    if (line_width_ind >= 0) {
      prop_indexes_.emplace_back(
          util::chart::Series::P_LINE_WIDTH,
          line_width_ind);
    }

    int point_style_ind = stmt->getComputedColumnIndex("pointstyle");
    if (point_style_ind >= 0) {
      prop_indexes_.emplace_back(
          util::chart::Series::P_POINT_STYLE,
          point_style_ind);
    }

    int point_size_ind = stmt->getComputedColumnIndex("pointsize");
    if (point_size_ind >= 0) {
      prop_indexes_.emplace_back(
          util::chart::Series::P_POINT_SIZE,
          point_size_ind);
    }

    bool first = true;
    Vector<SValue> row(result_cursor->getNumColumns());
    while (result_cursor->next(row.data(), row.size())) {
      if (first) {
        first = false;

        if (x_ind_ < 0) {
          RAISE(
              kRuntimeError,
              "can't draw SELECT because it has no 'x' column");
        }

        if (adapter_.get() == nullptr) {
          adapter_.reset(mkSeriesAdapter(const_cast<SValue*>(row.data())));
        } else {
          adapter_->name_ind_ = name_ind_;
          adapter_->x_ind_ = x_ind_;
          adapter_->y_ind_ = y_ind_;
          adapter_->z_ind_ = z_ind_;
        }

        adapter_->prop_indexes_ = prop_indexes_;
      }

      adapter_->nextRow(row.data(), row.size());
    }
  }

  virtual util::chart::Drawable* getChart() const = 0;
  virtual std::string chartName() const = 0;

protected:

  AnySeriesAdapter* mkSeriesAdapter(SValue* row) {
    AnySeriesAdapter* a = nullptr;
    if (!a) a = mkSeriesAdapter1D<SValue::TimeType>(row);
    if (!a) a = mkSeriesAdapter1D<SValue::FloatType>(row);
    if (!a) a = mkSeriesAdapter1D<SValue::StringType>(row);

    if (a == nullptr) {
      RAISE(kRuntimeError, "can't build seriesadapter");
    }

    return a;
  }

  template <typename TX>
  AnySeriesAdapter* mkSeriesAdapter1D(SValue* row) {
    if (!row[x_ind_].isConvertibleTo<TX>()) {
      return nullptr;
    }

    if (y_ind_ < 0) {
      return nullptr;
    }

    AnySeriesAdapter* a = nullptr;
    if (!a) a = mkSeriesAdapter2DStrict<TX, SValue::TimeType>(row);
    if (!a) a = mkSeriesAdapter2D<TX, SValue::FloatType>(row);
    if (!a) a = mkSeriesAdapter2D<TX, SValue::StringType>(row);
    return a;
  }

  template <typename TX, typename TY>
  AnySeriesAdapter* mkSeriesAdapter2D(SValue* row) {
    if (!row[y_ind_].isConvertibleTo<TY>()) {
      return nullptr;
    }

    if (z_ind_ < 0) {
      return new SeriesAdapter2D<TX, TY>(
          name_ind_,
          x_ind_,
          y_ind_);
    }

    AnySeriesAdapter* a = nullptr;
    if (!a) a = mkSeriesAdapter3DStrict<TX, TY, SValue::TimeType>(row);
    if (!a) a = mkSeriesAdapter3D<TX, TY, SValue::FloatType>(row);
    if (!a) a = mkSeriesAdapter3D<TX, TY, SValue::StringType>(row);
    return a;
  }

  template <typename TX, typename TY, typename TZ>
  AnySeriesAdapter* mkSeriesAdapter3D(SValue* row) {
    if (!row[z_ind_].isConvertibleTo<TZ>()) {
      return nullptr;
    }

    return new SeriesAdapter3D<TX, TY, TZ>(
        name_ind_,
        x_ind_,
        y_ind_,
        z_ind_);
  }

  template <typename TX, typename TY>
  AnySeriesAdapter* mkSeriesAdapter2DStrict(SValue* row) {
    if (!row[y_ind_].isOfType<TY>()) {
      return nullptr;
    }

    if (z_ind_ < 0) {
      return new SeriesAdapter2D<TX, TY>(
          name_ind_,
          x_ind_,
          y_ind_);
    }

    AnySeriesAdapter* a = nullptr;
    if (!a) a = mkSeriesAdapter3D<TX, TY, SValue::TimeType>(row);
    if (!a) a = mkSeriesAdapter3D<TX, TY, SValue::FloatType>(row);
    if (!a) a = mkSeriesAdapter3D<TX, TY, SValue::StringType>(row);
    return a;
  }

  template <typename TX, typename TY, typename TZ>
  AnySeriesAdapter* mkSeriesAdapter3DStrict(SValue* row) {
    if (!row[z_ind_].isOfType<TZ>()) {
      return nullptr;
    }

    return new SeriesAdapter3D<TX, TY, TZ>(
        name_ind_,
        x_ind_,
        y_ind_,
        z_ind_);
  }

  template <typename T>
  T* tryType2D() const {
    return tryTypeND<T, SeriesAdapter2D<
        typename T::TX,
        typename T::TY>>();
  }

  template <typename T>
  T* tryType3D() const {
    return tryTypeND<T, SeriesAdapter3D<
        typename T::TX,
        typename T::TY,
        typename T::TZ>>();
  }

  template <typename T, typename S>
  T* tryTypeND() const {
    auto adapter = dynamic_cast<S*>(adapter_.get());

    if (adapter == nullptr) {
      return nullptr;
    } else {
      auto chart = canvas_->addChart<T>();

      for (const auto& series : adapter->series_list_) {
        chart->addSeries(series);
      }

      return chart;
    }
  }

  void preconditionCheck() const {
    if (adapter_.get() == nullptr) {
      RAISE(
          kRuntimeError,
          "can't DRAW %s because the query returned no result rows",
          chartName().c_str());
    }
  }

  void invalidType() const {
    RAISE(
        kRuntimeError,
        "invalid series type for %s",
        chartName().c_str()); // FIXPAUL
  }

  std::unique_ptr<AnySeriesAdapter> adapter_;
  int name_ind_;
  int x_ind_;
  int y_ind_;
  int z_ind_;
  std::vector<std::pair<util::chart::Series::kProperty, int>> prop_indexes_;
  util::chart::Canvas* canvas_;
  RefPtr<DrawStatementNode> draw_stmt_;
};


}
