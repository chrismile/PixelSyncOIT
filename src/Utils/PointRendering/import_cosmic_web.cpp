#include <string>
#include <algorithm>
#include <limits>
#include <fstream>
#include "import_cosmic_web.h"

using namespace pl;

#pragma pack(1)
struct CosmicWebHeader {
	// number of particles in this dat file
	int np_local;
	float a, t, tau;
	int nts;
	float dt_f_acc, dt_pp_acc, dt_c_acc;
	int cur_checkpoint, cur_projection, cur_halofind;
	float massp;
};
std::ostream& operator<<(std::ostream &os, const CosmicWebHeader &h) {
	os << "{\n\tnp_local = " << h.np_local
		<< "\n\ta = " << h.a
		<< "\n\tt = " << h.t
		<< "\n\ttau = " << h.tau
		<< "\n\tnts = " << h.nts
		<< "\n\tdt_f_acc = " << h.dt_f_acc
		<< "\n\tdt_pp_acc = " << h.dt_pp_acc
		<< "\n\tdt_c_acc = " << h.dt_c_acc
		<< "\n\tcur_checkpoint = " << h.cur_checkpoint
		<< "\n\tcur_halofind = " << h.cur_halofind
		<< "\n\tmassp = " << h.massp
		<< "\n}";
	return os;
}

void pl::import_cosmic_web(const FileName &file_name, ParticleModel &model) {
	std::ifstream fin(file_name.c_str(), std::ios::binary);

	if (!fin.good()) {
		throw std::runtime_error("could not open particle data file " + file_name.file_name);
	}

	CosmicWebHeader header;
	if (!fin.read(reinterpret_cast<char*>(&header), sizeof(CosmicWebHeader))) {
		throw std::runtime_error("Failed to read header");
	}

	std::cout << "Cosmic Web Header: " << header << "\n";

	// Compute the brick offset for this file, given in the last 3 numbers of the name
	std::string brick_name = file_name.name();
	brick_name = brick_name.substr(brick_name.size() - 3, 3);
	const int brick_number = std::stoi(brick_name);

	// The cosmic web bricking is 8^3
	const int brick_z = brick_number / 64;
	const int brick_y = (brick_number / 8) % 8;
	const int brick_x = brick_number % 8;
	std::cout << "Brick position = { " << brick_x << ", " << brick_y
		<< ", " << brick_z << " }\n";

	// Each cell is 768x768x768 units
	const float step = 768.f;
	const vec3f offset(step * brick_x, step * brick_y, step * brick_z);

	auto positions = std::make_shared<DataT<float>>();
	auto velocities = std::make_shared<DataT<float>>();
	positions->data.reserve(header.np_local * 3);
	velocities->data.reserve(header.np_local * 3);

	std::vector<char> file_data(header.np_local * 2 * sizeof(vec3f), 0);
	if (!fin.read(file_data.data(), file_data.size())) {
		throw std::runtime_error("Failed to read cosmic web file");
	}

	vec3f *vecs = reinterpret_cast<vec3f*>(file_data.data());

	for (int i = 0; i < header.np_local; ++i) { 
		const vec3f position = vecs[i * 2] + offset;
		const vec3f velocity = vecs[i * 2 + 1];

		positions->data.push_back(position.x);
		positions->data.push_back(position.y);
		positions->data.push_back(position.z);

		velocities->data.push_back(velocity.x);
		velocities->data.push_back(velocity.y);
		velocities->data.push_back(velocity.z);
	}

	model["positions"] = std::move(positions);
	model["velocities"] = std::move(velocities);
}

