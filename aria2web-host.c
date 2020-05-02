#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <pthread.h>

#include <webview.h>
#define HTTPSERVER_IMPL
#include <httpserver.h>
#include <uriparser2.h>

void uri_unescape_in_place(char* p) {
	int dst = 0;
	for (int i = 0; p[i]; i++) {
		if (p[i] == '%') {
			if (p[i + 1] && p[i + 2]) {
				p[dst++] = (p[i + 1] - 0x30) * 16 + p[i + 2] - 0x30;
				i += 2;
			}
			else
				p[dst++] = '%';
		}
		else
			p[dst++] = p[i];
	}
	p[dst] = NULL;
}

void handle_request(struct http_request_s* request) {
	struct http_response_s* response = http_response_init();

	struct http_string_s target = http_request_target(request);
	char* targetPath = calloc(target.len + 2, 1);
	// FIXME: make sure that the requested resource is under pwd.
	targetPath[0] = '.';
	memcpy(targetPath + 1, target.buf, target.len);
	targetPath[target.len + 1] = NULL;
	puts(targetPath);
	uri_unescape_in_place(targetPath);
	puts(targetPath);

	int fd = open(targetPath, O_RDONLY);
	free(targetPath);
	if (fd == -1) {
		puts("... was NOT found.");
		http_response_status(response, 404);
		http_respond(request, response);
	} else {
		struct stat st;
		fstat(fd, &st);
		void* content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		assert(content);
		
		http_response_status(response, 200);
		http_response_header(response, "Content-Type", "text/html");
		http_response_body(response, (const char*) content, st.st_size);
		http_respond(request, response);
		munmap(content, st.st_size);
	}
}

void runHttpServer() {
	struct http_server_s* server = http_server_init(37564, handle_request);
	http_server_listen(server);
}

pthread_t http_server_thread;

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main() {
#endif
	pthread_create(&http_server_thread, NULL, runHttpServer, NULL);

	webview::webview w(true, nullptr);
	w.set_title("Minimal example");
	w.set_size(1200, 450, WEBVIEW_HINT_NONE);
	w.navigate("http://localhost:37564/index.html");
	w.run();
	
	return 0;
}

