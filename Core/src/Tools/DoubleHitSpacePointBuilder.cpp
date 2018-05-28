// This file is part of the ACTS project.
//
// Copyright (C) 2018 ACTS project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ACTS/Tools/DoubleHitSpacePointBuilder.hpp"
#include <cmath>
#include <limits>

///
/// @note Used abbreviation: "Strip Detector Element" -> SDE
///

double
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    differenceOfHits(const Acts::Vector3D& pos1,
                     const Acts::Vector3D& pos2,
                     const std::shared_ptr<Acts::DoubleHitSpacePointConfig> cfg)
{
  // Check if measurements are close enough to each other
  if ((pos1 - pos2).norm() > cfg->diffDist) return -1.;

  // Calculate the angles of the hits
  double phi1, theta1, phi2, theta2;
  phi1   = (pos1 - cfg->vertex).phi();
  theta1 = (pos1 - cfg->vertex).theta();
  phi2   = (pos2 - cfg->vertex).phi();
  theta2 = (pos2 - cfg->vertex).theta();

  // Calculate the squared difference between the theta angles
  double diffTheta2 = (theta1 - theta2) * (theta1 - theta2);
  if (diffTheta2 > cfg->diffTheta2) return -1.;

  // Calculate the squared difference between the phi angles
  double diffPhi2 = (phi1 - phi2) * (phi1 - phi2);
  if (diffPhi2 > cfg->diffPhi2) return -1.;

  // Return the squared distance between both hits
  return diffTheta2 + diffPhi2;
}

std::vector<Acts::BinningData>
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    binningData(Acts::PlanarModuleCluster const* hit)
{
  // Receive the binning
  auto& sur     = hit->referenceSurface();
  auto  segment = dynamic_cast<const Acts::CartesianSegmentation*>(
      &(sur.associatedDetectorElement()->digitizationModule()->segmentation()));
  return segment->binUtility().binningData();
}

std::pair<size_t, size_t>
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    binOfHit(Acts::PlanarModuleCluster const* hit)
{
  // Calculate the local coordinates of the hit
  const Acts::Vector2D local = localCoords(*hit);

  auto binData = binningData(hit);

  // Search the x-/y-bin hit
  size_t binX = binData[0].searchLocal(local);
  size_t binY = binData[1].searchLocal(local);

  return std::make_pair(binX, binY);
}

std::vector<std::vector<Acts::PlanarModuleCluster const*>>
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    sortHits(const std::vector<Acts::PlanarModuleCluster const*>& hits)
{

  auto* surface = &(hits[0]->referenceSurface());

  // Create a matrix of hits out of the collection of hits
  std::vector<std::vector<Acts::PlanarModuleCluster const*>> bins;

  // Resize the matrix
  auto binData = binningData(hits[0]);

  bins.resize(binData[0].bins());
  for (unsigned int index = 0; index < bins.size(); index++)
    bins[index].resize(binData[1].bins());

  // Fill the hits in the matrix
  for (unsigned int iHits = 0; iHits < hits.size(); iHits++) {
    // Check if all hits are from the same surface. This is necessary for
    // clustering based on bin numbers.
    if (&(hits[iHits]->referenceSurface()) != surface) {
      // Return empty matrix if multiple surfaces were given
      bins.clear();
      return bins;
    }
    std::pair<size_t, size_t> bin = binOfHit(hits[iHits]);
    bins[bin.first][bin.second] = hits[iHits];
  }

  return std::move(bins);
}

const std::vector<std::pair<Acts::PlanarModuleCluster const*,
                            Acts::PlanarModuleCluster const*>>
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    clusterSpacePoints(
        const std::vector<Acts::PlanarModuleCluster const*>& hits,
        bool                                                 performClustering)
{
  /// This function is a slow and stable sorting algorithm to cluster a
  /// collection of hits on the detector module. The idea is a combination of
  /// two hits in neighbouring strips. This follows the assumption that a
  /// particle can hit at most two strips.
  /// Since the hits can be provided in an arbitrary order, this function starts
  /// with the creation of a matrix of hits. This allows an easy look up
  /// afterwards by checking if a hit has also a hit on the previous bin. If
  /// this is the case both hits will be stored.

  std::vector<std::pair<Acts::PlanarModuleCluster const*,
                        Acts::PlanarModuleCluster const*>>
      clusters;

  // Easy exit if a single hit is provided
  if(hits.size() == 1)
  {
	clusters.push_back(std::make_pair(hits[0], nullptr));
	return clusters;
  }

  if (performClustering) {	  
    // Create a matrix of hits out of the collection of hits
    std::vector<std::vector<Acts::PlanarModuleCluster const*>> bins
        = sortHits(hits);

    // Empty matrix means that hits were from different surfaces and therefore
    // cannot be combined
    if (bins.empty()) return clusters;

	std::pair<Acts::PlanarModuleCluster const*, Acts::PlanarModuleCluster const*> cluster;

    // Check the orientation of a strip module = check which dimension has more
    // bins
    if (bins.size() > bins[0].size())
      // Walk through all bins
      for (unsigned int iY = 0; iY < bins[0].size(); iY++)
        for (unsigned int iX = 0; iX < bins.size(); iX++) {
			// Store the first hit
			if(bins[iX][iY] && !cluster.first)
			{
				cluster.first = bins[iX][iY];
				if(iX == bins.size() - 1)
				{
					cluster.second = nullptr;
					clusters.push_back(cluster);
				}
				continue;
			}
			if(cluster.first)
			{
				// Store the pointer to the second hit; nullptr if it does not exist
				cluster.second = bins[iX][iY];
				clusters.push_back(cluster);
				// Reset pair
				cluster.first = cluster.second;
			}
        }
    else
      // Perform the same computation as before with exchanged dimensions
      for (unsigned int iX = 0; iX < bins.size(); iX++)
        for (unsigned int iY = 0; iY < bins[0].size(); iY++) {
			// Store the first hit
			if(bins[iX][iY] && !cluster.first)
			{
				cluster.first = bins[iX][iY];
				if(iY == bins[0].size() - 1)
				{
					cluster.second = nullptr;
					clusters.push_back(cluster);
				}
				continue;
			}
			if(cluster.first)
			{
				// Store the pointer to the second hit; nullptr if it does not exist
				cluster.second = bins[iX][iY];
				clusters.push_back(cluster);
				// Reset pair
				cluster.first = cluster.second;
			}
        }
  } else
    // No clustering means that every hit is its own cluster
    for (auto& hit : hits) clusters.push_back(std::make_pair(hit, nullptr));

  return std::move(clusters);
}

const Acts::Vector3D
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    clusterPoint(std::pair<Acts::PlanarModuleCluster const*,
                           Acts::PlanarModuleCluster const*> cluster)
{
  // Get the hit coordinates
  Acts::Vector3D pos1 = globalCoords(*(cluster.first));

  if (cluster.second) {
    Acts::Vector3D pos2 = globalCoords(*(cluster.second));

    // Calculate the mean of both hits
    pos1(0) = (pos1(0) + pos2(0)) / 2;
    pos1(1) = (pos1(1) + pos2(1)) / 2;
    pos1(2) = (pos1(2) + pos2(2)) / 2;
  }
  return pos1;
}

void
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    addHits(std::vector<Acts::DoubleHitSpacePoint>&                spacePoints,
            const std::vector<Acts::PlanarModuleCluster const*>&   hitsFront,
            const std::vector<Acts::PlanarModuleCluster const*>&   hitsBack,
            const std::shared_ptr<Acts::DoubleHitSpacePointConfig> cfg)
{
  // Return if no hits are given in a vector
  if (hitsFront.empty() || hitsBack.empty()) return;

  // Test if config exists
  std::shared_ptr<Acts::DoubleHitSpacePointConfig> dhCfg;
  if (cfg)
    dhCfg = cfg;
  else
    // Use default config
    dhCfg = std::make_shared<Acts::DoubleHitSpacePointConfig>(
        Acts::DoubleHitSpacePointConfig());

  // Cluster hits and exit if no clusters are given
  auto clustersFront = clusterSpacePoints(hitsFront, dhCfg->clusterFrontHits);
  if (clustersFront.empty()) return;
  auto clustersBack = clusterSpacePoints(hitsBack, dhCfg->clusterBackHits);
  if (clustersBack.empty()) return;

  // Declare helper variables
  double         currentDiff;
  double         diffMin;
  unsigned int   clusterMin;
  Acts::Vector3D clusterPointFront;
  Acts::Vector3D clusterPointBack;

  // Walk through all hits on both surfaces
  for (unsigned int iClustersFront = 0; iClustersFront < clustersFront.size();
       iClustersFront++) {
    clusterPointFront = clusterPoint(clustersFront[iClustersFront]);
    // Set the closest distance to the maximum of double
    diffMin = std::numeric_limits<double>::max();
    // Set the corresponding index to an element not in the list of hits
    clusterMin = clustersBack.size();
    for (unsigned int iClustersBack = 0; iClustersBack < clustersBack.size();
         iClustersBack++) {
      clusterPointBack = clusterPoint(clustersBack[iClustersBack]);
      // Calculate the distances between the hits
      currentDiff
          = differenceOfHits(clusterPointFront, clusterPointBack, dhCfg);
      // Store the closest hits (distance and index) calculated so far
      if (currentDiff < diffMin && currentDiff >= 0.) {
        diffMin    = currentDiff;
        clusterMin = iClustersBack;
      }
    }

    // Store the best (=closest) result
    if (clusterMin < clustersBack.size()) {
      Acts::DoubleHitSpacePoint tmpSpacePoint;
      tmpSpacePoint.hitModuleFront = clustersFront[iClustersFront];
      tmpSpacePoint.hitModuleBack  = clustersBack[clusterMin];
      spacePoints.push_back(tmpSpacePoint);
    }
  }
}

std::pair<Acts::Vector3D, Acts::Vector3D>
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    endsOfStrip(const Acts::PlanarModuleCluster& hit)
{
  // Calculate the local coordinates of the hit
  const Acts::Vector2D local = localCoords(hit);

  // Receive the binning
  auto& sur     = hit.referenceSurface();
  auto  segment = dynamic_cast<const Acts::CartesianSegmentation*>(
      &(sur.associatedDetectorElement()->digitizationModule()->segmentation()));
  auto& binData     = segment->binUtility().binningData();
  auto& boundariesX = binData[0].boundaries();
  auto& boundariesY = binData[1].boundaries();

  // Search the x-/y-bin hit
  size_t binX = binData[0].searchLocal(local);
  size_t binY = binData[1].searchLocal(local);

  Acts::Vector2D topLocal, bottomLocal;

  if (boundariesX[binX + 1] - boundariesX[binX]
      < boundariesY[binY + 1] - boundariesY[binY]) {
    // Set the top and bottom end of the strip in local coordinates
    topLocal = {(boundariesX[binX] + boundariesX[binX + 1]) / 2,
                boundariesY[binY + 1]};
    bottomLocal
        = {(boundariesX[binX] + boundariesX[binX + 1]) / 2, boundariesY[binY]};
  } else {
    // Set the top and bottom end of the strip in local coordinates
    topLocal
        = {boundariesX[binX], (boundariesY[binY] + boundariesY[binY + 1]) / 2};
    bottomLocal = {boundariesX[binX + 1],
                   (boundariesY[binY] + boundariesY[binY + 1]) / 2};
  }

  // Calculate the global coordinates of the top and bottom end of the strip
  Acts::Vector3D topGlobal, bottomGlobal, mom;  // mom is a dummy variable
  sur.localToGlobal(topLocal, mom, topGlobal);
  sur.localToGlobal(bottomLocal, mom, bottomGlobal);

  // Return the top and bottom end of the strip in global coordinates
  return std::make_pair(topGlobal, bottomGlobal);
}

std::pair<Acts::Vector3D, Acts::Vector3D>
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    endsOfCluster(std::pair<PlanarModuleCluster const*,
                            PlanarModuleCluster const*> cluster)
{
  // Get the end of the strip(s)
  auto ends1 = endsOfStrip(*(cluster.first));
  if (cluster.second) {
    auto ends2 = endsOfStrip(*(cluster.second));

    // Calculate the mean of both ends
    ends1.first(0) = (ends1.first(0) + ends2.first(0)) / 2;
    ends1.first(1) = (ends1.first(1) + ends2.first(1)) / 2;
    ends1.first(2) = (ends1.first(2) + ends2.first(2)) / 2;

    ends1.second(0) = (ends1.second(0) + ends2.second(0)) / 2;
    ends1.second(1) = (ends1.second(1) + ends2.second(1)) / 2;
    ends1.second(2) = (ends1.second(2) + ends2.second(2)) / 2;
  }
  return ends1;
}

double
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    calcPerpProj(const Acts::Vector3D& a,
                 const Acts::Vector3D& c,
                 const Acts::Vector3D& q,
                 const Acts::Vector3D& r)
{
  /// This approach assumes that no vertex is available. This option aims to
  /// approximate the space points from cosmic data.
  /// The underlying assumption is that the best point is given by the closest
  /// distance between both lines describing the SDEs.
  /// The point x on the first SDE is parametrized as a + lambda0 * q with the
  /// top end a of the strip and the vector q = a - b(ottom end of the strip).
  /// An analogous parametrization is performed of the second SDE with y = c +
  /// lambda1 * r.
  /// x get resolved by resolving lambda0 from the condition that |x-y| is the
  /// shortest distance between two skew lines.
  Acts::Vector3D ac    = c - a;
  double         qr    = q.dot(r);
  double         denom = q.dot(q) - qr * qr;

  // Check for numerical stability
  if (fabs(denom) > 10e-7)
    // Return lambda0
    return (ac.dot(r) * qr - ac.dot(q) * r.dot(r)) / denom;
  // lambda0 is in the interval [-1,0]. This return serves as error check.
  return 1.;
}

bool
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    recoverSpacePoint(
        Acts::SpacePointBuilder<DoubleHitSpacePoint,
                                DoubleHitSpacePointConfig>::
            SpacePointParameters&                              spaPoPa,
        const std::shared_ptr<Acts::DoubleHitSpacePointConfig> cfg)
{
  /// Consider some cases that would allow an easy exit
  // Check if the limits are allowed to be increased
  if (cfg->stripLengthGapTolerance <= 0.) return false;
  spaPoPa.qmag = spaPoPa.q.mag();
  // Increase the limits. This allows a check if the point is just slightly
  // outside the SDE
  spaPoPa.limitExtended
      = spaPoPa.limit + cfg->stripLengthGapTolerance / spaPoPa.qmag;
  // Check if m is just slightly outside
  if (fabs(spaPoPa.m) > spaPoPa.limitExtended) return false;
  // Calculate n if not performed previously
  if (spaPoPa.n == 0.)
    spaPoPa.n = -spaPoPa.t.dot(spaPoPa.qs) / spaPoPa.r.dot(spaPoPa.qs);
  // Check if n is just slightly outside
  if (fabs(spaPoPa.n) > spaPoPa.limitExtended) return false;

  /// The following code considers an overshoot of m and n in the same direction
  /// of their SDE. The term "overshoot" represents the amount of m or n outside
  /// its regular interval (-1, 1).
  /// It calculates which overshoot is worse. In order to compare both, the
  /// overshoot in n is projected onto the first surface by considering the
  /// normalized projection of r onto q.
  /// This allows a rescaling of the overshoot. The worse overshoot will be set
  /// to +/-1, the parameter with less overshoot will be moved towards 0 by the
  /// worse overshoot.
  /// In order to treat both SDEs equally, the rescaling eventually needs to be
  /// performed several times. If these shifts allows m and n to be in the
  /// limits, the space point can be stored.
  /// @note This shift can be understood as a shift of the particle's
  /// trajectory. This is leads to a shift of the vertex. Since these two points
  /// are treated independently from other measurement, it is also possible to
  /// consider this as a change in the slope of the particle's trajectory. The
  /// would also move the vertex position.

  // Calculate the scaling factor to project lengths of the second SDE on the
  // first SDE
  double secOnFirstScale
      = spaPoPa.q.dot(spaPoPa.r) / (spaPoPa.qmag * spaPoPa.qmag);
  // Check if both overshoots are in the same direction
  if (spaPoPa.m > 1. && spaPoPa.n > 1.) {
    // Calculate the overshoots
    double mOvershoot = spaPoPa.m - 1.;
    double nOvershoot
        = (spaPoPa.n - 1.) * secOnFirstScale;  // Perform projection
    // Resolve worse overshoot
    double biggerOvershoot
        = (mOvershoot > nOvershoot) ? mOvershoot : nOvershoot;
    // Move m and n towards 0
    spaPoPa.m -= biggerOvershoot;
    spaPoPa.n -= (biggerOvershoot / secOnFirstScale);
    // Check if this recovered the space point
    return fabs(spaPoPa.m) < spaPoPa.limit && fabs(spaPoPa.n) < spaPoPa.limit;
  }
  // Check if both overshoots are in the same direction
  if (spaPoPa.m < -1. && spaPoPa.n < -1.) {
    // Calculate the overshoots
    double mOvershoot = -(spaPoPa.m + 1.);
    double nOvershoot
        = -(spaPoPa.n + 1.) * secOnFirstScale;  // Perform projection
    // Resolve worse overshoot
    double biggerOvershoot
        = (mOvershoot > nOvershoot) ? mOvershoot : nOvershoot;
    // Move m and n towards 0
    spaPoPa.m += biggerOvershoot;
    spaPoPa.n += (biggerOvershoot / secOnFirstScale);
    // Check if this recovered the space point
    return fabs(spaPoPa.m) < spaPoPa.limit && fabs(spaPoPa.n) < spaPoPa.limit;
  }
  // No solution could be found
  return false;
}

void
Acts::SpacePointBuilder<Acts::DoubleHitSpacePoint,
                        Acts::DoubleHitSpacePointConfig>::
    calculateSpacePoints(
        std::vector<Acts::DoubleHitSpacePoint>& spacePointStorage,
        const std::shared_ptr<Acts::DoubleHitSpacePointConfig> cfg)
{

  /// Source of algorithm: Athena, SiSpacePointMakerTool::makeSCT_SpacePoint()

  // Test if config exists
  std::shared_ptr<Acts::DoubleHitSpacePointConfig> dhCfg;
  if (cfg)
    dhCfg = cfg;
  else
    // Use default config
    dhCfg = std::make_shared<Acts::DoubleHitSpacePointConfig>(
        Acts::DoubleHitSpacePointConfig());

  Acts::SpacePointBuilder<DoubleHitSpacePoint,
                          DoubleHitSpacePointConfig>::SpacePointParameters
      spaPoPa;

  // Walk over every found candidate pair
  for (auto& hits : spacePointStorage) {

    // If the space point is already calculated this can be skipped
    if (hits.spacePoint != Acts::Vector3D::Zero(3)) continue;

    // Calculate the ends of the SDEs
    const auto& ends1 = endsOfCluster(hits.hitModuleFront);
    const auto& ends2 = endsOfCluster(hits.hitModuleBack);

    /// The following algorithm is meant for finding the position on the first
    /// strip if there is a corresponding hit on the second strip. The
    /// resulting point is a point x on the first surfaces. This point is
    /// along a line between the points a (top end of the strip)
    /// and b (bottom end of the strip). The location can be parametrized as
    /// 	2 * x = (1 + m) a + (1 - m) b
    /// as function of the scalar m. m is a parameter in the interval
    /// -1 < m < 1 since the hit was on the strip. Furthermore, the vector
    /// from the vertex to the hit on the second strip y is needed to be a
    /// multiple k of the vector from vertex to the hit on the first strip x.
    /// As a consequence of this demand y = k * x needs to be on the
    /// connecting line between the top (c) and bottom (d) end of
    /// the second strip. If both hits correspond to each other, the condition
    /// 	y * (c X d) = k * x (c X d) = 0 ("X" represents a cross product)
    /// needs to be fulfilled. Inserting the first equation into this
    /// equation leads to the condition for m as given in the following
    /// algorithm and therefore to the calculation of x.
    /// The same calculation can be repeated for y. Its corresponding
    /// parameter will be named n.

    spaPoPa.reset();
    spaPoPa.q = ends1.first - ends1.second;
    spaPoPa.r = ends2.first - ends2.second;

    // Fast skipping if a perpendicular projection should be used
    double resultPerpProj;
    if (dhCfg->usePerpProj
        && (resultPerpProj
            = calcPerpProj(ends1.first, ends2.first, spaPoPa.q, spaPoPa.r)
                <= 0.)) {
      hits.spacePoint = ends1.first + resultPerpProj * spaPoPa.q;
      continue;
    }

    spaPoPa.s  = ends1.first + ends1.second - 2 * dhCfg->vertex;
    spaPoPa.t  = ends2.first + ends2.second - 2 * dhCfg->vertex;
    spaPoPa.qs = spaPoPa.q.cross(spaPoPa.s);
    spaPoPa.rt = spaPoPa.r.cross(spaPoPa.t);
    spaPoPa.m  = -spaPoPa.s.dot(spaPoPa.rt) / spaPoPa.q.dot(spaPoPa.rt);

    // Set the limit for the parameter
    if (spaPoPa.limit == 1. && dhCfg->stripLengthTolerance != 0.)
      spaPoPa.limit = 1. + dhCfg->stripLengthTolerance;

    // Check if m and n can be resolved in the interval (-1, 1)
    if (fabs(spaPoPa.m) <= spaPoPa.limit
        && fabs(spaPoPa.n
                = -spaPoPa.t.dot(spaPoPa.qs) / spaPoPa.r.dot(spaPoPa.qs))
            <= spaPoPa.limit)
      // Store the space point
      hits.spacePoint
          = 0.5 * (ends1.first + ends1.second + spaPoPa.m * spaPoPa.q);
    else
        /// If this point is reached then it was not possible to resolve both
        /// points such that they are on their SDEs
        /// The following code treats a possible recovery of points resolved
        /// slightly outside of the SDE.
        /// @note This procedure is an indirect variation of the vertex
        /// position.
        // Check if a recovery the point(s) and store them if successful
        if (recoverSpacePoint(spaPoPa, dhCfg))
      hits.spacePoint
          = 0.5 * (ends1.first + ends1.second + spaPoPa.m * spaPoPa.q);
  }
}
