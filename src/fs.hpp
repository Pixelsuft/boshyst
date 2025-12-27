#pragma once
#include <string>

namespace bfs {
	class File {
	private:
		void* handle;
	public:
		File(const std::string& path, int mode) noexcept;
		bool is_open();
		bool read_line(std::string& line);
		bool write(const void* buf, size_t size);
		inline bool write_line(const std::string& line) {
			return write((line + '\n').c_str(), line.size() + 1);
		}
		inline void* get_handle() {
			return handle;
		}
		void close();
		inline ~File() {
			close();
		}
		File(const File&) = delete;
		File& operator=(const File&) = delete;
		File(File&& other) noexcept;
		File& operator=(File&& other) noexcept;
	};
}
