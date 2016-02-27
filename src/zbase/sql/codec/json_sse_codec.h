/**
 * This file is part of the "libfnord" project
 *   Copyright (c) 2015 Paul Asmuth
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <stx/stdtypes.h>
#include <csql/runtime/ResultFormat.h>
#include <stx/http/HTTPSSEStream.h>

namespace csql {

class JSONSSEStreamFormat : public ResultFormat {
public:

  JSONSSEStreamFormat();

  void formatResults(
      ScopedPtr<QueryPlan> query,
      ExecutionContext* context) override;

protected:
  RefPtr<http::HTTPSSEStream> output_;
};

class ASCIICodec {
public:

  ASCIICodec(
      csql::QueryPlan* query,
      RefPtr<http::HTTPSSEStream> output);

protected:

  void flushResult(size_t idx);

  RefPtr<http::HTTPSSEStream> output_;
  Vector<ScopedPtr<csql::ResultList>> results_;
};

}