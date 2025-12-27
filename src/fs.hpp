#pragma once
#include <string>

namespace bfs {
	class File {
	private:
		void* handle;
	public:
		File(const std::string& path, int mode);
		bool is_open();
		bool read_line(std::string& line);
		inline void* get_handle() {
			return handle;
		}
		void close();
		inline ~File() {
			close();
		}
	};
}
