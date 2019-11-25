#include <cassert>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include "tinyxml2.h"
#include "import_uintah.h"

using namespace pl;
using namespace tinyxml2;

struct UintahPatch {
	vec3f lower;
};

bool uintah_is_big_endian = false;

std::string tinyxml_error_string(const XMLError e){
	switch (e){
		case XML_NO_ATTRIBUTE:
			return "XML_NO_ATTRIBUTE";
		case XML_WRONG_ATTRIBUTE_TYPE:
			return "XML_WRONG_ATTRIBUTE_TYPE";
		case XML_ERROR_FILE_NOT_FOUND:
			return "XML_ERROR_FILE_NOT_FOUND";
		case XML_ERROR_FILE_COULD_NOT_BE_OPENED:
			return "XML_ERROR_FILE_COULD_NOT_BE_OPENED";
		case XML_ERROR_FILE_READ_ERROR:
			return "XML_ERROR_FILE_READ_ERROR";
		case XML_ERROR_PARSING_ELEMENT:
			return "XML_ERROR_PARSING_ELEMENT";
		case XML_ERROR_PARSING_ATTRIBUTE:
			return "XML_ERROR_PARSING_ATTRIBUTE";
		case XML_ERROR_PARSING_TEXT:
			return "XML_ERROR_PARSING_TEXT";
		case XML_ERROR_PARSING_CDATA:
			return "XML_ERROR_PARSING_CDATA";
		case XML_ERROR_PARSING_COMMENT:
			return "XML_ERROR_PARSING_COMMENT";
		case XML_ERROR_PARSING_DECLARATION:
			return "XML_ERROR_PARSING_DECLARATION";
		case XML_ERROR_PARSING_UNKNOWN:
			return "XML_ERROR_PARSING_UNKNOWN";
		case XML_ERROR_EMPTY_DOCUMENT:
			return "XML_ERROR_EMPTY_DOCUMENT";
		case XML_ERROR_MISMATCHED_ELEMENT:
			return "XML_ERROR_MISMATCHED_ELEMENT";
		case XML_ERROR_PARSING:
			return "XML_ERROR_PARSING";
		case XML_CAN_NOT_CONVERT_TEXT:
			return "XML_CAN_NOT_CONVERT_TEXT";
		case XML_NO_TEXT_NODE:
			return "XML_NO_TEXT_NODE";
		// XML_SUCCESS and XML_NO_ERROR both return success
		default:
			return "XML_SUCCESS";
	}
}
double ntohd(double d){
	double ret = 0;
	char *in = reinterpret_cast<char*>(&d);
	char *out = reinterpret_cast<char*>(&ret);
	for (int i = 0; i < 8; ++i){
		out[i] = in[7 - i];
	}
	return ret;
}
float ntohf(float f){
	float ret = 0;
	char *in = reinterpret_cast<char*>(&f);
	char *out = reinterpret_cast<char*>(&ret);
	for (int i = 0; i < 4; ++i){
		out[i] = in[3 - i];
	}
	return ret;
}
bool read_particles(const FileName &file_name, Data *pos_data, const size_t num_particles,
		const size_t start, const size_t end) {
	auto *positions = static_cast<DataT<float>*>(pos_data);
	assert(positions);
	// TODO: Would mmap'ing the file at the start and keeping each new file we encounter
	// mapped be faster than fopen/fread/fclose?
	FILE *fp = fopen(file_name.file_name.c_str(), "rb");
	if (!fp){
		std::cout << "Failed to open Uintah data file '" << file_name << "'\n";
		return false;
	}
	fseek(fp, start, SEEK_SET);
	size_t len = end - start;
	if (len != num_particles * sizeof(double) * 3){
		std::cout << "Length of data != expected length of particle data\n";
		return false;
		fclose(fp);
	}

	double position[3];
	for (size_t i = 0; i < num_particles; ++i){
		if (fread(position, sizeof(position), 1, fp) != 1){
			std::cout << "Error reading particle from file\n";
			fclose(fp);
			return false;
		}
		if (uintah_is_big_endian){
			position[0] = ntohd(position[0]);
			position[1] = ntohd(position[1]);
			position[2] = ntohd(position[2]);
		}
		positions->data.push_back(static_cast<float>(position[0]));
		positions->data.push_back(static_cast<float>(position[1]));
		positions->data.push_back(static_cast<float>(position[2]));
	}
	fclose(fp);
	return true;
}
template<typename In, typename Out = In>
bool read_particle_attribute(const FileName &file_name, Data *attrib_data, const size_t num_particles,
		const size_t start, const size_t end){
	auto attribs = static_cast<DataT<Out>*>(attrib_data);
	// TODO: Would mmap'ing the file at the start and keeping each new file we encounter
	// mapped be faster than fopen/fread/fclose?
	FILE *fp = fopen(file_name.file_name.c_str(), "rb");
	if (!fp){
		std::cout << "Failed to open Uintah data file '" << file_name << "'\n";
		return false;
	}
	fseek(fp, start, SEEK_SET);
	size_t len = end - start;
	if (len != num_particles * sizeof(In)){
		std::cout << "Length of data != expected length of particle data\n";
		return false;
		fclose(fp);
	}

	std::vector<In> data(num_particles, In());
	if (fread(data.data(), sizeof(In), num_particles, fp) != num_particles){
		std::cout << "Error reading particle attribute from file\n";
		fclose(fp);
		return false;
	}
	fclose(fp);
	std::transform(data.begin(), data.end(), std::back_inserter(attribs->data),
			[](const In &t){ return static_cast<Out>(t); });
	return true;
}
bool read_uintah_particle_variable(const FileName &base_path, XMLElement *elem,
		ParticleModel &model)
{
	std::string type;
	{
		const char *type_attrib = elem->Attribute("type");
		if (!type_attrib){
			std::cout << "Variable missing type attribute\n";
			return false;
		}
		type = std::string(type_attrib);
	}
	std::string variable;
	std::string file_name;
	// Note: might need uint64_t if we want to run on windows as long there is only
	// 4 bytes
	size_t index = std::numeric_limits<size_t>::max();
	size_t start = std::numeric_limits<size_t>::max();
	size_t end = std::numeric_limits<size_t>::max();
	size_t patch = std::numeric_limits<size_t>::max();
	size_t num_particles = 0;
	for (XMLNode *c = elem->FirstChild(); c; c = c->NextSibling()){
		XMLElement *e = c->ToElement();
		if (!e){
			std::cout << "Failed to parse Uintah variable element\n";
			return false;
		}
		std::string name = e->Value();
		const char *text = e->GetText();
		if (!text){
			std::cout << "Invalid variable '" << name << "', missing value\n";
			return false;
		}
		if (name == "variable"){
			variable = text;
		} else if (name == "filename"){
			file_name = text;
		} else if (name == "index"){
			try {
				index = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid index value specified\n";
				return false;
			}
		}  else if (name == "start"){
			try {
				start = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid start value specified\n";
				return false;
			}
		} else if (name == "end"){
			try {
				end = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid end value specified\n";
				return false;
			}
		} else if (name == "patch"){
			try {
				patch = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid patch value specified\n";
				return false;
			}
		} else if (name == "numParticles"){
			try {
				num_particles = std::strtoul(text, NULL, 10);
			} catch (const std::range_error &r){
				std::cout << "Invalid numParticles value specified\n";
				return false;
			}
		}
	}
	if (num_particles > 0){
		if (variable == "p.x") {
			variable = "positions";
		}
		const bool need_new_array = model.find(variable) == model.end();
		// Particle positions are p.x, rename them to position when we load them
		// TODO: This should handle arbitrary ParticleVariable<Point> types
		if (variable == "positions"){
			if (need_new_array) {
				std::cout << "new positions array\n";
				model["positions"] = std::make_shared<DataT<float>>();
			}
			return read_particles(base_path.join(FileName(file_name)),
					model["positions"].get(), num_particles, start, end);
		} else if (type == "ParticleVariable<double>"){
			if (need_new_array) {
				model[variable] = std::make_shared<DataT<double>>();
			}
			return read_particle_attribute<double>(base_path.join(FileName(file_name)),
					model[variable].get(), num_particles, start, end);
		} else if (type == "ParticleVariable<float>"){
			if (need_new_array) {
				model[variable] = std::make_shared<DataT<float>>();
			}
			return read_particle_attribute<float>(base_path.join(FileName(file_name)),
					model[variable].get(), num_particles, start, end);
		} else if (type == "ParticleVariable<long64>"){
			if (need_new_array) {
				model[variable] = std::make_shared<DataT<int64_t>>();
			}
			return read_particle_attribute<int64_t>(base_path.join(FileName(file_name)),
					model[variable].get(), num_particles, start, end);
		}
	}
	return true;
}
bool read_uintah_datafile(const FileName &file_name, XMLDocument &doc, ParticleModel &model){
	XMLElement *node = doc.FirstChildElement("Uintah_Output");
	const static std::string VAR_TYPE = "ParticleVariable";
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		if (std::string(c->Value()) != "Variable"){
			std::cout << "Invalid XML node encountered, expected <Variable...>\n";
			return false;
		}
		XMLElement *e = c->ToElement();
		if (!e || !e->Attribute("type")){
			std::cout << "Invalid variable element found\n";
			return false;
		}
		std::string var_type = e->Attribute("type");
		if (var_type.substr(0, VAR_TYPE.size()) == VAR_TYPE){
			if (!read_uintah_particle_variable(file_name.path(), e, model)){
				return false;
			}
		}
	}
	return true;
}
bool read_uintah_timestep_meta(XMLNode *node){
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		XMLElement *e = c->ToElement();
		if (!e){
			std::cout << "Error parsing Uintah timestep meta\n";
			return false;
		}
		if (std::string(e->Value()) == "endianness"){
			if (std::string(e->GetText()) == "big_endian"){
				std::cout << "Uintah parser switching to big endian\n";
				uintah_is_big_endian = true;
			}
		}
	}
	return true;
}
bool read_uintah_patches(XMLNode *node, std::vector<UintahPatch> &patches) {
	// TODO: How to deal with multiple levels?
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		if (std::string(c->Value()) == "Level") {
			for (XMLNode *p = c->FirstChild(); p; p = p->NextSibling()) {
				if (std::string(p->Value()) == "Patch") {
					UintahPatch patch;
					XMLElement *lower_elem = p->FirstChildElement("lower");
					std::sscanf(lower_elem->GetText(), "[%f, %f, %f]",
							&patch.lower.x, &patch.lower.y, &patch.lower.z);
					patches.push_back(patch);
				}
			}
		}
	}
	return true;
}
bool read_uintah_timestep_data(const FileName &base_path, XMLNode *node,
		ParticleModel &model)
{
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		if (std::string(c->Value()) == "Datafile"){
			XMLElement *e = c->ToElement();
			if (!e){
				std::cout << "Error parsing Uintah timestep data\n";
				return false;
			}
			const char *href = e->Attribute("href");
			if (!href){
				std::cout << "Error parsing Uintah timestep data: Missing file href\n";
				return false;
			}
			FileName data_file = base_path.join(FileName(std::string(href)));
			std::cout << "Reading " << data_file << "\n";
			XMLDocument doc;
			XMLError err = doc.LoadFile(data_file.file_name.c_str());
			if (err != XML_SUCCESS){
				std::cout << "Error loading Uintah data file '" << data_file << "': "
					<< tinyxml_error_string(err) << "\n";
				return false;
			}
			if (!read_uintah_datafile(data_file, doc, model)){
				std::cout << "Error reading Uintah data file " << data_file << "\n";
				return false;
			}
		}
	}
	return true;
}
bool read_uintah_timestep(const FileName &file_name, XMLElement *node, ParticleModel &model){
	std::vector<UintahPatch> patches;
	for (XMLNode *c = node->FirstChild(); c; c = c->NextSibling()){
		std::cout << c->Value() << "\n" << std::flush;
		const std::string node_type = c->Value();
		if (node_type == "Meta") {
			if (!read_uintah_timestep_meta(c)){
				return false;
			}
		}
	}
	XMLNode *c = node->FirstChildElement("Data");
	if (!c || !read_uintah_timestep_data(file_name.path(), c, model)){
		return false;
	}
	return true;
}

void pl::import_uintah(const FileName &file_name, ParticleModel &model){
	std::cout << "Importing Uintah data from " << file_name << "\n";
	XMLDocument doc;
	XMLError err = doc.LoadFile(file_name.file_name.c_str());
	if (err != XML_SUCCESS){
		std::cout << "Error loading Uintah data file '" << file_name << "': "
			<< tinyxml_error_string(err) << "\n";
		throw std::runtime_error("Failed to open XML file");
	}
	if (doc.FirstChildElement("Uintah_timestep")) {
		if (!read_uintah_timestep(file_name, doc.FirstChildElement("Uintah_timestep"), model)) {
			std::cout << "Error reading Uintah timestep\n";
			throw std::runtime_error("Failed to read Uintah timestep");
		}
	} else if (doc.FirstChildElement("Uintah_Output")) {
		if (!read_uintah_datafile(file_name, doc, model)) {
			std::cout << "Error reading Uintah Output\n";
			throw std::runtime_error("Failed to read Uintah output");
		}
	} else {
		std::cout << "Unrecognized UDA XML file!\n";
		throw std::runtime_error("Failed to read Uintah data");
	}
	if (model.find("positions") != model.end()) {
		auto positions = static_cast<DataT<float>*>(model["positions"].get());
		std::cout << "Read Uintah data with " << positions->data.size() / 3 << " particles\n";
	} else {
		std::cout << "Warning! File " << file_name << " contained no particles\n";
	}
}

