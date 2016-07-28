#ifndef __TINY_WORLD_H
#define __TINY_WORLD_H

#include <iostream>
#include <cmath>
#include <memory>

#include "../core/state_data.h"
#include "../core/sensor_data.h"
#include "../core/laser_scan_grid_world.h"
#include "../core/maps/grid_cell_strategy.h"

#include "tiny_grid_cells.h"
#include "tiny_scan_matcher.h"

/*!
 * \brief A container for the following tinySLAM parameters:
 * TODO: params description
 */
struct TinyWorldParams {
  double localized_scan_quality, raw_scan_quality;
};

/*!
 * \brief Derived class from LaserScanGridWorld to estimate occupancy of each cell
 *
 * There is an robot state handle based on used scanner matcher rules and laser data handle based on algorithm from the article with wall blur
 */
class TinyWorld : public LaserScanGridWorld {
private: // internal params
  // Scan matcher
  const double SIG_XY = 0.2;
  const double SIG_TH = 0.1;
  const double BAD_LMT = 20;
  const double TOT_LMT = BAD_LMT * 5;

  const double HOLE_WIDTH = 1.5;
public:
  using Point = DiscretePoint2D;
public:

  /*!
   * Initialize the world to produce tiny SLAM
   * \param[in] gcs    - shared pointer on strategy for each cell (how does estimate occupancy, cost of laser data etc)
   * \param[in] params - the initial values of trust for laser data
   */
  TinyWorld(std::shared_ptr<GridCellStrategy> gcs,
            const TinyWorldParams &params) :
    LaserScanGridWorld(gcs), _gcs(gcs), _params(params),
    _scan_matcher(new TinyScanMatcher(_gcs->cost_est(),
                                      BAD_LMT, TOT_LMT,
                                      SIG_XY, SIG_TH)) {}

  /*!
   * Updates robot pose and map by prediction-correction scheme.
   * 
   * The scan quality used for map update depends on whether the robot pose have been changed during correction step.
   * \param[in] scan - data from laser scanner
   */
  virtual void handle_observation(TransformedLaserScan &scan) override {
    RobotState pose_delta;
    _scan_matcher->reset_state();
    _scan_matcher->process_scan(pose(), scan, map(), pose_delta);
    update_robot_pose(pose_delta.x, pose_delta.y, pose_delta.theta);

    bool pose_was_fixed = pose_delta.x || pose_delta.y || pose_delta.theta;
    auto factory = _gcs->cell_factory();
    scan.quality = pose_was_fixed ? _params.localized_scan_quality :
                                    _params.raw_scan_quality;
    LaserScanGridWorld::handle_observation(scan);
  }

  /*!
   * Updates the map with a given laser scan point.
   * 
   * estimates the occupancy of the cell where obstacle locates (beam_end_x, beam_end_y).
   * And after there is a "wall blur"
   * \param[in] map - all built map
   * \param[in] laser_x,laser_y - the beginning coordinates of the laser ray
   * \param[in] beam_end_x, beam_end_y - the ending coordinates of the laser ray
   * \param[in] is_occ - the parameter which shows is this cell occupied or not
   * \param[in] quality - the quality of laser scanner
   */
  virtual void handle_scan_point(GridMap &map,
                                 double laser_x, double laser_y,
                                 double beam_end_x, double beam_end_y,
                                 bool is_occ, double quality) override {
    Beam beam{laser_x, laser_y, beam_end_x, beam_end_y};
    Point robot_pt = map.world_to_cell(laser_x, laser_y);
    Point obst_pt = map.world_to_cell(beam_end_x, beam_end_y);

    double obst_dist_sq = robot_pt.dist_sq(obst_pt);
    std::vector<Point> pts = DiscreteLine2D(robot_pt, obst_pt).points();
    double hole_dist_sq = std::pow(HOLE_WIDTH / map.cell_scale(), 2);

    auto occ_est = _gcs->occupancy_est();
    Occupancy beam_end_occ = occ_est->estimate_occupancy(beam,
        map.world_cell_bounds(pts.back()), is_occ);
    map.update_cell(pts.back(), beam_end_occ, quality);
    pts.pop_back();

    for (const auto &pt : pts) {
      Occupancy empty_cell_value = occ_est->estimate_occupancy(beam,
          map.world_cell_bounds(pts.back()), false);
      // wall blur
      double dist_sq = pt.dist_sq(obst_pt);
      if (dist_sq < hole_dist_sq && hole_dist_sq < obst_dist_sq && is_occ) {
        empty_cell_value.prob_occ +=
          (1.0 - dist_sq / hole_dist_sq) * beam_end_occ.prob_occ;
      }

      map.update_cell(pt, empty_cell_value, quality);
    }
  }

  /*!
   * Returns the scan matcher used for robot pose correction.
   */
  std::shared_ptr<GridScanMatcher> scan_matcher() {
    return _scan_matcher;
  }
private:
  std::shared_ptr<GridCellStrategy> _gcs;
  const TinyWorldParams _params;
  std::shared_ptr<TinyScanMatcher> _scan_matcher;
};

#endif
