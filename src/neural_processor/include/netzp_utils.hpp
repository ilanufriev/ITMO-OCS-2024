#ifndef _NETZP_UTILS_H_
#define _NETZP_UTILS_H_

#include "netzp_config.hpp"
#include <cstring>

template <typename T>
struct DataVector {
    using value_type = T;
    std::vector<value_type> data;

    DataVector() = default;
    DataVector(const DataVector& other) = default;

    bool operator == (const DataVector& other) const {
        return data == other.data;
    }
};

template <typename T>
std::ostream& operator<<(std::ostream& out, const DataVector<T>& vector) {
    out << "DataVector { ";
    for (const T& el : vector.data) {
        out << el << " ";
    }
    out << "}";

    return out;
}

template <typename PrimitiveType>
std::vector<uchar> ToBytesVector(PrimitiveType value) {
    std::vector<uchar> result(sizeof(PrimitiveType));
    memcpy(result.data(), &value, sizeof(PrimitiveType));
    return result;
}

#endif // _NETZP_UTILS_H_