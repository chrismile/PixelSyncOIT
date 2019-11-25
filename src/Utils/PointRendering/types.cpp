#include <ostream>
#include "types.h"

using namespace pl;

vec3f::vec3f(float x) : x(x), y(x), z(x) {}
vec3f::vec3f(float x, float y, float z) : x(x), y(y), z(z) {}
vec3f& vec3f::operator+=(const vec3f &a) {
	x += a.x;
	y += a.y;
	z += a.z;
	return *this;
}

vec3f operator+(const vec3f &a, const vec3f &b){
	return vec3f(a.x + b.x, a.y + b.y, a.z + b.z);
}
vec3f operator*(const vec3f &a, const vec3f &b){
	return vec3f(a.x * b.x, a.y * b.y, a.z * b.z);
}
vec3f operator-(const vec3f &a, const vec3f &b){
	return vec3f(a.x - b.x, a.y - b.y, a.z - b.z);
}
vec3f operator/(const vec3f &a, const vec3f &b){
	return vec3f(a.x / b.x, a.y / b.y, a.z / b.z);
}
std::ostream& operator<<(std::ostream &os, const vec3f &v){
	os << "[ " << v.x << ", " << v.y << ", " << v.z << " ]";
	return os;
}

FileName::FileName(const std::string &file_name) : file_name(file_name){
	normalize_separators();
}
FileName::FileName(const char *file_name) : file_name(file_name) {
	normalize_separators();
}
FileName& FileName::operator=(const std::string &name) {
	file_name = name;
	normalize_separators();
	return *this;
}
const char* FileName::c_str() const {
	return file_name.c_str();
}
FileName FileName::path() const {
	size_t fnd = file_name.rfind("/");
	if (fnd != std::string::npos){
		return FileName(file_name.substr(0, fnd + 1));
	}
	return FileName("");
}
bool FileName::empty() const {
	return file_name.empty();
}
std::string FileName::extension() const {
	size_t fnd = file_name.rfind(".");
	if (fnd != std::string::npos){
		return file_name.substr(fnd + 1);
	}
	return "";
}
FileName FileName::join(const FileName &other) const {
	return FileName(file_name + "/" + other.file_name);
}
std::string FileName::name() const {
	std::string n = file_name;
	size_t fnd = n.rfind("/");
	if (fnd != std::string::npos) {
		n = n.substr(fnd + 1);
	}
	fnd = n.rfind(".");
	if (fnd != std::string::npos) {
		n = n.substr(0, fnd);
	}
	return n;
}
void FileName::normalize_separators() {
#ifdef _WIN32
	for (char &c : file_name) {
		if (c == '\\') {
			c = '/';
		}
	}
#endif
}
std::ostream& operator<<(std::ostream &os, const FileName &f){
	os << f.file_name;
	return os;
}

bool pl::starts_with(const std::string &a, const std::string &prefix) {
	if (a.size() < prefix.size()) {
		return false;
	}
	return std::equal(prefix.begin(), prefix.end(), a.begin());
}

