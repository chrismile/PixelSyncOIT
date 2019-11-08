#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include <memory>
#include <iostream>
#include <typeinfo>

namespace pl {

template<typename T>
T clamp(T x, T lo, T hi){
	if (x < lo){
		return lo;
	}
	if (x > hi){
		return hi;
	}
	return x;
}

struct vec3f {
	float x, y, z;

	// TODO Will: Why did I initialize these to 1?
	vec3f(float x = 1.0);
	vec3f(float x, float y, float z);

	vec3f& operator+=(const vec3f &a);
};

struct FileName {
	std::string file_name;

	FileName() = default;
	FileName(const std::string &file_name);
	FileName(const char *file_name);
	FileName& operator=(const std::string &file_name);
	FileName path() const;
	const char* c_str() const;
	bool empty() const;
	std::string extension() const;
	FileName join(const FileName &other) const;
	std::string name() const;

private:
	void normalize_separators();
};

struct Data {
	virtual const std::type_info& type() const = 0;
	// Dump the data in binary format to the output stream as raw data
	virtual void write(std::ofstream &os) const = 0;
	// Get the element at i as a float
	virtual float get_float(const size_t i) const = 0;
	// Get the number of elements in the array
	virtual size_t size() const = 0;
	virtual ~Data(){}
};

template<typename T>
struct DataT : Data {
	std::vector<T> data;

	const std::type_info& type() const override {
		return typeid(T);
	}
	void write(std::ofstream &os) const override {
		std::cout << "Writing " << data.size() << " " << typeid(T).name()
			<< ", file is " << sizeof(T) * data.size() << " bytes\n";
		os.write(reinterpret_cast<const char*>(data.data()), sizeof(T) * data.size());
	}
	float get_float(const size_t i) const override {
		return static_cast<float>(data[i]);
	}
	size_t size() const override {
		return data.size();
	}
};

using ParticleModel = std::unordered_map<std::string, std::shared_ptr<Data>>;

bool starts_with(const std::string &a, const std::string &prefix);

}

pl::vec3f operator+(const pl::vec3f &a, const pl::vec3f &b);
pl::vec3f operator*(const pl::vec3f &a, const pl::vec3f &b);
pl::vec3f operator-(const pl::vec3f &a, const pl::vec3f &b);
pl::vec3f operator/(const pl::vec3f &a, const pl::vec3f &b);
std::ostream& operator<<(std::ostream &os, const pl::vec3f &v);
std::ostream& operator<<(std::ostream &os, const pl::FileName &f);

