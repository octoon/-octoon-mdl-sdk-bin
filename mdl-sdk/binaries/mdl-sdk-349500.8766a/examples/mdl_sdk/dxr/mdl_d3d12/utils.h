/******************************************************************************
 * Copyright 2022 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/

// examples/mdl_sdk/dxr/mdl_d3d12/utils.h

#ifndef MDL_D3D12_UTILS_H
#define MDL_D3D12_UTILS_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cmath>
#include <mutex>
#include <algorithm>
#include <Windows.h>
#include <DirectXMath.h>

#define SRC __FILE__,__LINE__

struct ID3D12Object;
struct ID3D12Device;

namespace mi { namespace examples { namespace mdl_d3d12
{
    const float PI = 3.14159265358979323846f;
    const float PI_OVER_2 = PI * 0.5f;
    const float ONE_OVER_PI = 0.318309886183790671538f;
    const float SQRT_2 = 1.41421356237f;
    const float SQRT_3 = 1.73205080757f;

    void log_verbose(
        const std::string& message,
        const std::string& file = "",
        int line = 0);

    void log_info(
        const std::string& message,
        const std::string& file = "",
        int line = 0);

    void log_warning(
        const std::string& message,
        const std::string& file = "",
        int line = 0);

    void log_error(
        const std::string& message,
        const std::string& file = "",
        int line = 0);

    void log_error(
        const std::exception& exception,
        const std::string& file,
        int line = 0);

    void log_error(
        const std::string& message,
        const std::exception& exception,
        const std::string& file, int line);

    bool log_on_failure(
        HRESULT error_code,
        const std::string& message,
        const std::string& file = "",
        int line = 0);

    void throw_on_failure(
        HRESULT error_code,
        const std::string& message,
        const std::string& file = "",
        int line = 0);

    // flushes all ongoing async logging tasks. has to be called before closing the app.
    void flush_loggers();

    // set a log file to use or \c nullptr in order disable file logging when active.
    // has to be called with a nullptr before closing the app.
    void log_set_file_path(const char* log_file_path);

    void set_debug_name(ID3D12Object* obj, const std::string& name);

    void set_dred_device(ID3D12Device* device);

    inline size_t round_to_power_of_two(size_t value, size_t power_of_two_factor)
    {
        return (value + (power_of_two_factor - 1)) & ~(power_of_two_factor - 1);
    }

    // --------------------------------------------------------------------------------------------

    struct Timing
    {
        explicit Timing(std::string operation);
        ~Timing();

    private:
        std::string m_operation;
        std::chrono::steady_clock::time_point m_start;
    };

    // --------------------------------------------------------------------------------------------

    class Profiling
    {
    public:
        struct Measurement
        {
            ~Measurement();
        private:
            friend class Profiling;
            explicit Measurement(Profiling* p, std::string operation);

            Profiling* m_profiling;
            std::string m_operation;
            std::chrono::steady_clock::time_point m_start;
        };

        Measurement measure(std::string operation);
        void print_statistics() const;
        void reset_statistics() { m_statistics.clear(); }

    private:
        struct Entry
        {
            double average;
            size_t count;
        };

        void on_measured(const Measurement& m, const double& value);

        std::map<std::string, Entry> m_statistics;
        std::mutex m_statistics_mtx;
    };

    // --------------------------------------------------------------------------------------------
    // DirectX Math
    // --------------------------------------------------------------------------------------------

    inline DirectX::XMMATRIX inverse(const DirectX::XMMATRIX& m, DirectX::XMVECTOR* determinants = nullptr)
    {
        if (determinants)
            return DirectX::XMMatrixInverse(determinants, m);

        DirectX::XMVECTOR det;
        return DirectX::XMMatrixInverse(&det, m);
    }

    inline DirectX::XMFLOAT3 normalize(const DirectX::XMFLOAT3& v)
    {
        float inv_length = 1.0f / std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        return {v.x * inv_length, v.y * inv_length, v.z * inv_length};
    }

    inline float length(const DirectX::XMFLOAT3& v)
    {
        return std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    inline float length2(const DirectX::XMFLOAT3& v)
    {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    inline float dot(const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2)
    {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    }

    inline float average(const DirectX::XMFLOAT3& v)
    {
        return (v.x + v.y + v.z) * 0.33333333333333f;
    }

    inline float maximum(const DirectX::XMFLOAT3& v)
    {
        return std::max<float>(v.x, std::max<float>(v.y, v.z));
    }

    inline float minimum(const DirectX::XMFLOAT3& v)
    {
        return std::min<float>(v.x, std::min<float>(v.y, v.z));
    }

    inline DirectX::XMFLOAT3 cross(const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2)
    {
        DirectX::XMFLOAT3 res;
        res.x = (v1.y * v2.z) - (v1.z * v2.y);
        res.y = (v1.z * v2.x) - (v1.x * v2.z);
        res.z = (v1.x * v2.y) - (v1.y * v2.x);
        return res;
    }

    inline DirectX::XMFLOAT3 operator-(const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2)
    {
        DirectX::XMFLOAT3 res;
        res.x = v1.x - v2.x;
        res.y = v1.y - v2.y;
        res.z = v1.z - v2.z;
        return res;
    }

    inline DirectX::XMFLOAT3 operator+(const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2)
    {
        DirectX::XMFLOAT3 res;
        res.x = v1.x + v2.x;
        res.y = v1.y + v2.y;
        res.z = v1.z + v2.z;
        return res;
    }

    inline DirectX::XMFLOAT3 operator*(const DirectX::XMFLOAT3& v1, float s)
    {
        DirectX::XMFLOAT3 res;
        res.x = v1.x * s;
        res.y = v1.y * s;
        res.z = v1.z * s;
        return res;
    }

    inline void operator*=(DirectX::XMFLOAT3& v, float s)
    {
        v.x *= s;
        v.y *= s;
        v.z *= s;
    }

    inline void operator/=(DirectX::XMFLOAT3& v, float s)
    {
        v.x /= s;
        v.y /= s;
        v.z /= s;
    }

    inline DirectX::XMFLOAT3 operator-(const DirectX::XMFLOAT3& v1)
    {
        DirectX::XMFLOAT3 res;
        res.x = -v1.x;
        res.y = -v1.y;
        res.z = -v1.z;
        return res;
    }

    inline void operator+=(DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2)
    {
        v1.x += v2.x;
        v1.y += v2.y;
        v1.z += v2.z;
    }

    // --------------------------------------------------------------------------------------------
    // Diagnostics
    // --------------------------------------------------------------------------------------------
    class Diagnostics
    {
    private:
        Diagnostics();
    public:

        static void list_loaded_libraries();
    };

}}} // mi::examples::mdl_d3d12
#endif
