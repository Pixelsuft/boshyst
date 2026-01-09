#pragma once
#include <string>

#if _MSC_VER >= 1900
#define NOEXCEPT noexcept
#else
#define NOEXCEPT
#endif

namespace bfs {
	enum class SeekMode {
		Begin = 0,
		Current = 1,
		End = 2
	};

	class File {
	private:
		void* handle;
	public:
		File(const std::string& path, int mode) NOEXCEPT;
		bool is_open();
		bool read_line(std::string& line);
		bool read(void* buf, size_t size);
		bool write(const void* buf, size_t size);
		inline bool write(const std::string& data) {
			return write(data.data(), data.size());
		}
		inline bool write_line(const std::string& line) {
			return write((line + "\r\n").c_str(), line.size() + 2);
		}
		bool seek(long long offset, SeekMode mode = SeekMode::Begin);
		long long tell();
		inline void* get_handle() {
			return handle;
		}
		void close();
		inline ~File() {
			close();
		}
#if _MSC_VER >= 1900
		File(const File&) = delete;
		File& operator=(const File&) = delete;
#endif
		File(File&& other) NOEXCEPT;
		File& operator=(File&& other) NOEXCEPT;
	};
}

bool state_save(bfs::File* file);
bool state_load(bfs::File* file);
