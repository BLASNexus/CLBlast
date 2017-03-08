
// =================================================================================================
// This file is part of the CLBlast project. The project is licensed under Apache Version 2.0. This
// project loosely follows the Google C++ styleguide and uses a tab-size of two spaces and a max-
// width of 100 characters per line.
//
// Author(s):
//   Cedric Nugteren <www.cedricnugteren.nl>
//
// This file implements a class with static methods to describe the XaxpyBatched routine. Examples of
// such 'descriptions' are how to calculate the size a of buffer or how to run the routine. These
// static methods are used by the correctness tester and the performance tester.
//
// =================================================================================================

#ifndef CLBLAST_TEST_ROUTINES_XAXPYBATCHED_H_
#define CLBLAST_TEST_ROUTINES_XAXPYBATCHED_H_

#include <vector>
#include <string>

#include "utilities/utilities.hpp"

#ifdef CLBLAST_REF_CLBLAS
  #include "test/wrapper_clblas.hpp"
#endif
#ifdef CLBLAST_REF_CBLAS
  #include "test/wrapper_cblas.hpp"
#endif

namespace clblast {
// =================================================================================================

// See comment at top of file for a description of the class
template <typename T>
class TestXaxpyBatched {
 public:

  // Although it is a non-BLAS routine, it can still be tested against level-1 routines in a loop
  static size_t BLASLevel() { return 1; }

  // The list of arguments relevant for this routine
  static std::vector<std::string> GetOptions() {
    return {kArgN,
            kArgXInc, kArgYInc,
            kArgBatchCount, kArgAlpha};
  }

  // Helper to determine a different alpha value per batch
  static T GetAlpha(const T alpha_base, const size_t batch_id) {
    return alpha_base + Constant<T>(batch_id);
  }

  // Helper for the sizes per batch
  static size_t PerBatchSizeX(const Arguments<T> &args) { return args.n * args.x_inc; }
  static size_t PerBatchSizeY(const Arguments<T> &args) { return args.n * args.y_inc; }

  // Describes how to obtain the sizes of the buffers
  static size_t GetSizeX(const Arguments<T> &args) {
    return PerBatchSizeX(args) * args.batch_count + args.x_offset;
  }
  static size_t GetSizeY(const Arguments<T> &args) {
    return PerBatchSizeY(args) * args.batch_count + args.y_offset;
  }

  // Describes how to set the sizes of all the buffers
  static void SetSizes(Arguments<T> &args) {
    args.x_size = GetSizeX(args);
    args.y_size = GetSizeY(args);
    args.x_offsets = std::vector<size_t>(args.batch_count);
    args.y_offsets = std::vector<size_t>(args.batch_count);
    for (auto batch = size_t{0}; batch < args.batch_count; ++batch) {
      args.x_offsets[batch] = batch * PerBatchSizeX(args) + args.x_offset;
      args.y_offsets[batch] = batch * PerBatchSizeY(args) + args.y_offset;
    }
  }

  // Describes what the default values of the leading dimensions of the matrices are
  static size_t DefaultLDA(const Arguments<T> &) { return 1; } // N/A for this routine
  static size_t DefaultLDB(const Arguments<T> &) { return 1; } // N/A for this routine
  static size_t DefaultLDC(const Arguments<T> &) { return 1; } // N/A for this routine

  // Describes which transpose options are relevant for this routine
  using Transposes = std::vector<Transpose>;
  static Transposes GetATransposes(const Transposes &) { return {}; } // N/A for this routine
  static Transposes GetBTransposes(const Transposes &) { return {}; } // N/A for this routine

  // Describes how to prepare the input data
  static void PrepareData(const Arguments<T>&, Queue&, const int, std::vector<T>&,
                          std::vector<T>&, std::vector<T>&, std::vector<T>&, std::vector<T>&,
                          std::vector<T>&, std::vector<T>&) {} // N/A for this routine

  // Describes how to run the CLBlast routine
  static StatusCode RunRoutine(const Arguments<T> &args, Buffers<T> &buffers, Queue &queue) {
    auto queue_plain = queue();
    auto event = cl_event{};
    auto alphas = std::vector<T>();
    for (auto batch = size_t{0}; batch < args.batch_count; ++batch) {
      alphas.push_back(GetAlpha(args.alpha, batch));
    }
    auto status = AxpyBatched(args.n, alphas.data(),
                              buffers.x_vec(), args.x_offsets.data(), args.x_inc,
                              buffers.y_vec(), args.y_offsets.data(), args.y_inc,
                              args.batch_count,
                              &queue_plain, &event);
    if (status == StatusCode::kSuccess) { clWaitForEvents(1, &event); clReleaseEvent(event); }
    return status;
  }

  // Describes how to run the clBLAS routine (for correctness/performance comparison)
  #ifdef CLBLAST_REF_CLBLAS
    static StatusCode RunReference1(const Arguments<T> &args, Buffers<T> &buffers, Queue &queue) {
      auto queue_plain = queue();
      for (auto batch = size_t{0}; batch < args.batch_count; ++batch) {
        auto event = cl_event{};
        auto status = clblasXaxpy(args.n, GetAlpha(args.alpha, batch),
                                  buffers.x_vec, args.x_offsets[batch], args.x_inc,
                                  buffers.y_vec, args.y_offsets[batch], args.y_inc,
                                  1, &queue_plain, 0, nullptr, &event);
        clWaitForEvents(1, &event);
        if (static_cast<StatusCode>(status) != StatusCode::kSuccess) {
          return static_cast<StatusCode>(status);
        }
      }
      return StatusCode::kSuccess;
    }
  #endif

  // Describes how to run the CPU BLAS routine (for correctness/performance comparison)
  #ifdef CLBLAST_REF_CBLAS
    static StatusCode RunReference2(const Arguments<T> &args, Buffers<T> &buffers, Queue &queue) {
      std::vector<T> x_vec_cpu(args.x_size, static_cast<T>(0));
      std::vector<T> y_vec_cpu(args.y_size, static_cast<T>(0));
      buffers.x_vec.Read(queue, args.x_size, x_vec_cpu);
      buffers.y_vec.Read(queue, args.y_size, y_vec_cpu);
      for (auto batch = size_t{0}; batch < args.batch_count; ++batch) {
        cblasXaxpy(args.n, GetAlpha(args.alpha, batch),
                   x_vec_cpu, args.x_offsets[batch], args.x_inc,
                   y_vec_cpu, args.y_offsets[batch], args.y_inc);
      }
      buffers.y_vec.Write(queue, args.y_size, y_vec_cpu);
      return StatusCode::kSuccess;
    }
  #endif

  // Describes how to download the results of the computation
  static std::vector<T> DownloadResult(const Arguments<T> &args, Buffers<T> &buffers, Queue &queue) {
    std::vector<T> result(args.y_size, static_cast<T>(0));
    buffers.y_vec.Read(queue, args.y_size, result);
    return result;
  }

  // Describes how to compute the indices of the result buffer
  static size_t ResultID1(const Arguments<T> &args) { return args.n; }
  static size_t ResultID2(const Arguments<T> &args) { return args.batch_count; }
  static size_t GetResultIndex(const Arguments<T> &args, const size_t id1, const size_t id2) {
    return (id1 * args.y_inc) + args.y_offsets[id2];
  }

  // Describes how to compute performance metrics
  static size_t GetFlops(const Arguments<T> &args) {
    return args.batch_count * (2 * args.n);
  }
  static size_t GetBytes(const Arguments<T> &args) {
    return args.batch_count * (3 * args.n) * sizeof(T);
  }
};

// =================================================================================================
} // namespace clblast

// CLBLAST_TEST_ROUTINES_XAXPYBATCHED_H_
#endif
