// Copyright 2022 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCRT_CFX_READ_ONLY_VECTOR_STREAM_H_
#define CORE_FXCRT_CFX_READ_ONLY_VECTOR_STREAM_H_

#include <stdint.h>

#include "core/fxcrt/data_vector.h"
#include "core/fxcrt/fx_stream.h"
#include "core/fxcrt/retain_ptr.h"

class CFX_ReadOnlySpanStream;

class CFX_ReadOnlyVectorStream final : public IFX_SeekableReadStream {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;

  // IFX_SeekableReadStream:
  FX_FILESIZE GetSize() override;
  bool ReadBlockAtOffset(void* buffer,
                         FX_FILESIZE offset,
                         size_t size) override;

 private:
  explicit CFX_ReadOnlyVectorStream(DataVector<uint8_t> data);
  ~CFX_ReadOnlyVectorStream() override;

  const DataVector<uint8_t> data_;
  const RetainPtr<CFX_ReadOnlySpanStream> stream_;
};

#endif  // CORE_FXCRT_CFX_READ_ONLY_VECTOR_STREAM_H_
