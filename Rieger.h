// Author: Jake Rieger
// Created: 8/6/2024.
//

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <limits>
#include <filesystem>
#include <fstream>
#include <optional>

#define CAST static_cast
#define RCAST reinterpret_cast
#define CCAST const_cast
#define DCAST dynamic_cast

using u8   = uint8_t;
using u16  = uint16_t;
using u32  = uint32_t;
using u64  = uint64_t;
using i8   = int8_t;
using i16  = int16_t;
using i32  = int32_t;
using i64  = int64_t;
using f32  = float;
using f64  = double;
using wstr = std::wstring;
using str  = std::string;

using Exception    = std::exception;
using RuntimeError = std::runtime_error;

namespace FileSystem = std::filesystem;
using Path           = std::filesystem::path;

static constexpr std::nullopt_t kNone = std::nullopt;

template<class T>
using Shared = std::shared_ptr<T>;

template<class T>
using Option = std::optional<T>;

template<class T>
using Unique = std::unique_ptr<T>;

template<class T>
using Vector = std::vector<T>;

constexpr auto Inf32 = std::numeric_limits<float>::infinity();
constexpr auto Inf64 = std::numeric_limits<double>::infinity();

namespace IO {
    inline Option<str> Read(const Path& filename) {
        if (!exists(filename) || is_directory(filename)) {
            return kNone;
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            return kNone;
        }

        str content((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());
        file.close();

        return content;
    }

    inline Option<Vector<u8>> ReadAllBytes(const Path& filename) {
        if (!exists(filename) || is_directory(filename)) {
            return kNone;
        }

        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return kNone;
        }

        Vector<u8> bytes(std::istreambuf_iterator(file), {});
        file.close();

        return bytes;
    }

    inline Option<Vector<str>> ReadAllLines(const Path& filename) {
        if (!exists(filename) || is_directory(filename)) {
            return kNone;
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            return kNone;
        }

        Vector<str> lines;
        str line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }

        file.close();

        return lines;
    }

    inline Option<Vector<u8>>
    ReadBlock(const Path& filename, const u32 blockOffset, const size_t blockSize) {
        if (!exists(filename) || is_directory(filename)) {
            return kNone;
        }

        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return kNone;
        }

        file.seekg(blockOffset, std::ios::beg);
        if (!file.good()) {
            return kNone;
        }

        Vector<u8> buffer(blockSize);
        file.read(RCAST<char*>(buffer.data()), CAST<std::streamsize>(blockSize));

        if (file.gcount() != CAST<std::streamsize>(blockSize)) {
            return kNone;
        }

        return buffer;
    }

    inline bool Write(const Path& filename, const str& content) {
        std::ofstream outfile(filename);
        if (!outfile.is_open()) {
            return false;
        }

        outfile.write(content.c_str(), CAST<std::streamsize>(content.length()));
        outfile.close();

        return true;
    }

    inline bool WriteAllBytes(const Path& filename, const Vector<u8>& bytes) {
        std::ofstream outfile(filename, std::ios::binary);
        if (!outfile.is_open()) {
            return false;
        }

        outfile.write(RCAST<const char*>(bytes.data()), CAST<std::streamsize>(bytes.size()));
        outfile.close();

        return true;
    }

    inline bool WriteAllLines(const Path& filename, const Vector<str>& lines) {
        std::ofstream outfile(filename);
        if (!outfile.is_open()) {
            return false;
        }

        for (const auto& line : lines) {
            outfile << line << std::endl;
        }

        outfile.close();

        return true;
    }
}  // namespace IO

#if defined(_WIN32) || defined(_WIN64)

    #pragma warning(disable : 4996)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <codecvt>

namespace WindowsAPI {
    inline std::string GetHResultErrorMessage(HRESULT hr) {
        char* errorMsg = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr,
                       hr,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       RCAST<LPSTR>(&errorMsg),
                       0,
                       nullptr);

        std::string errorString;
        if (errorMsg) {
            errorString = errorMsg;
            LocalFree(errorMsg);
        } else {
            errorString = "Unknown error";
        }

        return errorString;
    }

    class ComError final : public std::exception {
    public:
        explicit ComError(HRESULT hr) noexcept : result(hr) {}

        [[nodiscard]] const char* what() const noexcept override {
            static char errMsg[256] = {};
            sprintf_s(errMsg,
                      "Failure with HRESULT of %08X.\nError: %s\n",
                      CAST<u32>(result),
                      GetHResultErrorMessage(result).c_str());
            return errMsg;
        }

    private:
        HRESULT result;
    };

    inline void ThrowIfFailed(HRESULT hr) {
        if (FAILED(hr)) {
            throw ComError(hr);
        }
    }

    enum class AlertSeverity : u32 {
        Info    = MB_ICONINFORMATION | MB_OK,
        Warning = MB_ICONWARNING | MB_OK,
        Error   = MB_ICONERROR | MB_OK,
    };

    inline void ShowMessage(const str& message,
                            const AlertSeverity severity,
                            const str& caption = "Alert",
                            HWND window        = nullptr) {
        ::MessageBoxA(window, message.c_str(), caption.c_str(), CAST<u32>(severity));
    }

    inline void WideToANSI(const wstr& value, str& converted) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        converted = converter.to_bytes(value);
    }

    inline void ANSIToWide(const str& value, wstr& converted) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        converted = converter.from_bytes(value);
    }
}  // namespace WindowsAPI
#endif

namespace Math {
    template<typename T>
    concept FiniteFloat = std::floating_point<T> && requires(T value) {
        { std::isfinite(value) } -> std::convertible_to<bool>;
    };

    template<FiniteFloat T>
    T Lerp(T a, T b, f64 t) {
        if (a == b) {
            return a;
        }

        auto result = a * (1.0 - t) + b * t;
        return CAST<T>(result);
    }
}  // namespace Math

namespace Color {
    inline void HexToRGBA(const u32 hex, f32& r, f32& g, f32& b, f32& a) {
        const u8 alphaByte = (hex >> 24) & 0xFF;
        const u8 redByte   = (hex >> 16) & 0xFF;
        const u8 greenByte = (hex >> 8) & 0xFF;
        const u8 blueByte  = hex & 0xFF;

        a = CAST<f32>(CAST<u32>(alphaByte) / 255.0);
        r = CAST<f32>(CAST<u32>(redByte) / 255.0);
        g = CAST<f32>(CAST<u32>(greenByte) / 255.0);
        b = CAST<f32>(CAST<u32>(blueByte) / 255.0);
    }

    inline void HexToRGBA(const u32 hex, u32& r, u32& g, u32& b, u32& a) {
        const u8 alphaByte = (hex >> 24) & 0xFF;
        const u8 redByte   = (hex >> 16) & 0xFF;
        const u8 greenByte = (hex >> 8) & 0xFF;
        const u8 blueByte  = hex & 0xFF;

        a = CAST<u32>(alphaByte);
        r = CAST<u32>(redByte);
        g = CAST<u32>(greenByte);
        b = CAST<u32>(blueByte);
    }

    inline u32 RGBAToHex(const f32 r, const f32 g, const f32 b, const f32 a) {
        const auto redByte   = CAST<u8>(r * 255.0f);
        const auto greenByte = CAST<u8>(g * 255.0f);
        const auto blueByte  = CAST<u8>(b * 255.0f);
        const auto alphaByte = CAST<u8>(a * 255.0f);

        return (alphaByte << 24) | (redByte << 16) | (greenByte << 8) | blueByte;
    }
}  // namespace Color