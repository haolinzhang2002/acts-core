// This file is part of the Acts project.
//
// Copyright (C) 2021 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Acts/EventData/detail/TransformationBoundToFree.hpp"
//#include <unsupported/Eigen/MatrixFunctions>

#include <type_traits>

namespace Acts {
namespace detail {

/// @brief Parameters sampling based on covariance matrix sqrt root
///
struct CorrectedBoundToFreeTransformer {
  /// The parameter to tune the weight
  ActsScalar kappa = 4;

  /// Get the non-linearity corrected bound parameters and its covariance
  std::optional<std::pair<FreeVector, FreeSymMatrix>> operator()(
      const BoundVector& boundParams, const BoundSymMatrix& boundCovariance,
      const Surface& surface, const GeometryContext& geoContext) {
    std::cout << "++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
    size_t sampleSize = 2 * eBoundSize + 1;
    std::vector<std::pair<BoundVector, ActsScalar>> sampledBoundParams;
    sampledBoundParams.reserve(sampleSize);

    // Initialize the covariance sqrt root matrix
    BoundSymMatrix covSqrt = BoundSymMatrix::Zero();
    //   std::cout << "boundCovariance =  \n" << boundCovariance << std::endl;
    // Get the covariance sqrt root matrix
    Eigen::JacobiSVD<BoundSymMatrix> svd(
        boundCovariance, Eigen::ComputeThinU | Eigen::ComputeThinV);
    // U*S*V^-1
    auto S = svd.singularValues();
    auto U = svd.matrixU();
    auto V = svd.matrixV();
    /*
        if (U != V) {
          std::cout << "U != V " << std::endl;
        }
        std::cout << "S = \n " << S << std::endl;
        std::cout << "U = \n " << U << std::endl;
        std::cout << "V = \n " << V << std::endl;
    */
    BoundMatrix D = BoundMatrix::Zero();
    for (int i = 0; i < eBoundSize; ++i) {
      D(i, i) = std::sqrt(S(i));
    }

    BoundMatrix UP = BoundMatrix::Zero();
    for (int i = 0; i < eBoundSize; ++i) {
      for (int j = 0; j < eBoundSize; ++j) {
        UP(i, j) = U(i, j);
      }
    }
    covSqrt = UP * D * UP.transpose();

    auto test = covSqrt * covSqrt;

    //  std::cout << "covSqrt = \n" << covSqrt << std::endl;

    // Sample the free parameters
    // 1. the baseline parameter
    sampledBoundParams.push_back({boundParams, (kappa - eBoundSize) / kappa});
    // 2. the shifted parameters
    for (size_t i = 0; i < eBoundSize; ++i) {
      ActsScalar kappaSqrt = std::sqrt(kappa);
      //     std::cout << "delta =  \n " << covSqrt.col(i) * kappaSqrt << std::endl;
      sampledBoundParams.push_back(
          {boundParams + covSqrt.col(i) * kappaSqrt, 0.5 / kappa});
      sampledBoundParams.push_back(
          {boundParams - covSqrt.col(i) * kappaSqrt, 0.5 / kappa});
    }

    //    std::cout<<"sampledBoundParams.size() " << sampledBoundParams.size()
    //    << std::endl;
    // Initialize the mean of the bound parameters
    FreeVector fpMean = FreeVector::Zero();
    // Initialize the bound covariance
    FreeSymMatrix fv = FreeSymMatrix::Zero();

    // The transformed bound parameters
    std::vector<FreeVector> transformedFreeParams;
    // Loop over the sample points of the free parameters to get the weighted
    // bound parameters
    for (const auto& [params, weight] : sampledBoundParams) {
      // Transform the bound to free
      auto fp =
          detail::transformBoundToFreeParameters(surface, geoContext, params);
      //      std::cout << "Transformation succeeded with transformed fp = \n" << fp << std::endl;
      transformedFreeParams.push_back(fp);

      fpMean += weight * fp;
    }

    //    std::cout << "transformedFreeParams.size() "
    //              << transformedFreeParams.size() << std::endl;
    if (transformedFreeParams.empty()) {
      return std::nullopt;
    }

    // Get the weighted bound covariance
    for (size_t isample = 0; isample < sampleSize; ++isample) {
      FreeVector sigma = transformedFreeParams[isample] - fpMean;

      //    std::cout<<"weight " << sampledBoundParams[isample].second << " for sigma = \n" << sigma<<std::endl;
      fv += sampledBoundParams[isample].second * sigma * sigma.transpose();
    }

    return std::pair<FreeVector, FreeSymMatrix>(fpMean, fv);
  }
};

}  // namespace detail
}  // namespace Acts
