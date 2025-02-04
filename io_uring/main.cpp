#include <liburing.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>

class unique_fd {
public:
    unique_fd(int fd) : m_fd(fd) {}
    unique_fd(unique_fd&& uf) { m_fd = uf.m_fd; uf.m_fd = -1; }
    ~unique_fd() { if (m_fd != -1) close(m_fd); }

    explicit operator bool() const { return m_fd != -1; }
    operator int() const { return m_fd; }

private:
    int m_fd;

    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;
};

int main(int argc, char *argv[])
{
    constexpr int queue_depth = 1;
    constexpr int block_size = 4096;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *file_path = argv[1];

    // 初始化 io_uring
    struct io_uring ring;
    int ret = io_uring_queue_init(queue_depth, &ring, 0);
    if (ret < 0) {
        errno = -ret;
        perror("io_uring_queue_init");
        return 1;
    }

    // 用智能指針來管理 io_uring 資源
    std::unique_ptr<io_uring, decltype(&io_uring_queue_exit)> ring_guard(&ring, &io_uring_queue_exit);

    // 開啟文件
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // // 用智能指針來管理文件描述符
    unique_fd fd_guard(fd);

    // 分配內存來保存讀取的數據
    char *buf = (char*)malloc(block_size);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    // 用智能指針來管理內存
    std::unique_ptr<char[], decltype(&free)> buf_guard(buf, &free);

    // 提交讀取請求
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "io_uring_get_sqe failed\n");
        return 1;
    }

    io_uring_prep_read(sqe, fd, buf, block_size, 0);
    io_uring_sqe_set_data(sqe, (void*)file_path);

    io_uring_submit(&ring);

    // 等待完成隊列條目
    struct io_uring_cqe *cqe;
    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
        errno = -ret;
        perror("io_uring_wait_cqe");
        return 1;
    }

    if (cqe->res < 0) {
        fprintf(stderr, "Async readv failed: %s\n", strerror(-cqe->res));
    } else {
        printf("%s Read %d bytes: %.*s\n", (char*)cqe->user_data, cqe->res, cqe->res, buf);
    }

    // 釋放完成隊列條目
    io_uring_cqe_seen(&ring, cqe);

    return 0;
}
