/*!
 * @file
 *
 * @section LICENSE
 *
 * Copyright (C) 2017 by the Georgia Tech Research Institute (GTRI)
 *
 * This file is part of SCRIMMAGE.
 *
 *   SCRIMMAGE is free software: you can redistribute it and/or modify it under
 *   the terms of the GNU Lesser General Public License as published by the
 *   Free Software Foundation, either version 3 of the License, or (at your
 *   option) any later version.
 *
 *   SCRIMMAGE is distributed in the hope that it will be useful, but WITHOUT
 *   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *   License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with SCRIMMAGE.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Kevin DeMarco <kevin.demarco@gtri.gatech.edu>
 * @author Eric Squires <eric.squires@gtri.gatech.edu>
 * @date 31 July 2017
 * @version 0.1.0
 * @brief Brief file description.
 * @section DESCRIPTION
 * A Long description goes here.
 *
 */

#include <scrimmage/plugins/motion/Unicycle/Unicycle.h>
#include <scrimmage/common/Utilities.h>
#include <scrimmage/parse/ParseUtils.h>
#include <scrimmage/plugin_manager/RegisterPlugin.h>
#include <scrimmage/entity/Entity.h>
#include <scrimmage/math/State.h>

#include <boost/algorithm/clamp.hpp>

REGISTER_PLUGIN(scrimmage::MotionModel, scrimmage::motion::Unicycle, Unicycle_plugin)

using boost::algorithm::clamp;

enum ModelParams {
    X = 0,
    Y,
    Z,
    YAW,
    PITCH,
    MODEL_NUM_ITEMS
};

namespace scrimmage {
namespace motion {

bool Unicycle::init(std::map<std::string, std::string> &info,
                    std::map<std::string, std::string> &params) {

    // Declare variables for controllers
    speed_idx_ = vars_.declare(VariableIO::Type::speed, VariableIO::Direction::In);
    turn_rate_idx_ = vars_.declare(VariableIO::Type::turn_rate, VariableIO::Direction::In);
    pitch_rate_idx_ = vars_.declare(VariableIO::Type::pitch_rate, VariableIO::Direction::In);

    x_.resize(MODEL_NUM_ITEMS);
    x_[X] = state_->pos()(0);
    x_[Y] = state_->pos()(1);
    x_[Z] = state_->pos()(2);
    x_[YAW] = state_->quat().yaw();
    x_[PITCH] = state_->quat().pitch();

    turn_rate_max_ = std::stod(params.at("turn_rate_max"));
    pitch_rate_max_ = std::stod(params.at("pitch_rate_max"));
    vel_max_ = std::stod(params.at("vel_max"));
    enable_roll_ = get<bool>("enable_roll", params, false);

    return true;
}

bool Unicycle::step(double t, double dt) {
    // Get inputs and saturate
    velocity_ = clamp(vars_.input(speed_idx_), -vel_max_, vel_max_);
    turn_rate_ = clamp(vars_.input(turn_rate_idx_), -turn_rate_max_, turn_rate_max_);
    pitch_rate_ = clamp(vars_.input(pitch_rate_idx_), -pitch_rate_max_, pitch_rate_max_);

    double prev_x = x_[X];
    double prev_y = x_[Y];
    double prev_z = x_[Z];

    ode_step(dt);

    double dx = (x_[X] - prev_x) / dt;
    double dy = (x_[Y] - prev_y) / dt;
    double dz = (x_[Z] - prev_z) / dt;

    state_->vel()(0) = dx;
    state_->vel()(1) = dy;
    state_->vel()(2) = dz;

    state_->pos()(0) = x_[X];
    state_->pos()(1) = x_[Y];
    state_->pos()(2) = x_[Z];

    double roll = 0; // TODO: simulate roll if enabled
    if (enable_roll_ && turn_rate_ != 0) {
        // see https://en.wikipedia.org/wiki/Standard_rate_turn
        const double radius = velocity_ / turn_rate_;
        roll = atan2(pow(velocity_, 2) / radius, g_);
    }
    state_->quat().set(roll, -x_[PITCH], x_[YAW]);

    return true;
}

void Unicycle::model(const vector_t &x , vector_t &dxdt , double t) {
    double xy_speed = velocity_ * cos(x[PITCH]);
    dxdt[X] = xy_speed * cos(x[YAW]);
    dxdt[Y] = xy_speed * sin(x[YAW]);
    dxdt[Z] = velocity_ * sin(x[PITCH]);
    dxdt[YAW] = turn_rate_;
    dxdt[PITCH] = pitch_rate_;
}
} // namespace motion
} // namespace scrimmage
