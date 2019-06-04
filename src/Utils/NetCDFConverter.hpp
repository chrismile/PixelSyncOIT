//
// Created by christoph on 25.09.18.
//

#ifndef NETCDFIMPORTER_NETCDFCONVERTER_HPP
#define NETCDFIMPORTER_NETCDFCONVERTER_HPP

#include <string>
#include "TrajectoryFile.hpp"

Trajectories loadNetCdfFile(const std::string &filename);

#endif //NETCDFIMPORTER_NETCDFCONVERTER_HPP
