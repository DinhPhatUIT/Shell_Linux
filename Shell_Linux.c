#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LINE 80    // Số ký tự tối đa trên 1 dòng
#define MAX_COMMAND 41 // Số command tối đa được sử dụng trong pipe, lệnh liên tiếp, ví dụ: 'ls | grep a | wc -l' là 3 command
#define DEFAULT_PID 1  // id mặc định để gán cho process chính khi hàm fork() chưa được gọi

char *args[MAX_LINE / 2 + 1]; // Mảng để lưu trữ lệnh và các tham số của lệnh
int argsCount;
pid_t pid = DEFAULT_PID;

// Hàm để kill tiến trình con
void sigint_handler()
{
    if (pid == 0)
    {
        // Nếu là tiến trình con, in ra dòng chữ này
        printf("\nCtrl + C pressed, quitting program...\n");
    }
}

void exec_cmd(char *command)
{
    argsCount = 0;

    // Xử lý chuỗi nhập vào
    char *token = strtok(command, " "); // Tách chuỗi ký tự đầu tiên ra khi có dấu cách và lưu vào token
    while (token != NULL)
    {
        args[argsCount++] = token; // args[i] bằng một chuỗi ký tự ngăn bởi dấu cách hay nói cách khác là một từ; có tối đa 41 từ trên một dòng
        token = strtok(NULL, " "); // Tiếp tục tách chuỗi đến khi token là ký tự rỗng khi bị cắt bởi dấu cách
    }
    args[argsCount] = NULL; // Từ cuối cùng sẽ là kết thúc chuỗi từ

    // Nếu lệnh là chuyển hướng đầu vào/ra
    int input_fd, output_fd;
    int redirect_input = 0, redirect_output = 0;

    for (int j = 0; j < argsCount; j++)
    {
        // Dấu hiệu chuyển hướng đầu ra của một tệp
        if (strcmp(args[j], ">") == 0)
        {
            args[j] = NULL;      // Đánh dấu vị trí chuyển hướng là NULL
            redirect_output = 1; // Cờ chuyển hướng đầu ra

            if (args[j + 1] == NULL)
            { // Nơi đầu ra được chuyển hướng vào là NULL nghĩa là không có file output.
                printf("Missing output file\n");
                exit(EXIT_FAILURE); // Báo lỗi xong thoát
            }
            // Tạo file args[i] nếu chưa có; file này sẽ thế chỗ đầu ra cho luồng chuẩn STDOUT
            output_fd = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // output_fd lưu trữ giá trị file descriptor nên phía trên mới khai báo int
            if (output_fd < 0)
            {
                perror("System fail to execute.");
                exit(EXIT_FAILURE); // Không mở được file nên báo lỗi rồi thoát
            }
            dup2(output_fd, STDOUT_FILENO); // Overwrite file STDOUT hiện tại bằng file output_fd vừa tạo, luồng đầu ra bây giờ là file output_fd
            close(output_fd);
        }

        // Dấu hiệu chuyển hướng đầu vào của một tệp
        else if (strcmp(args[j], "<") == 0)
        {
            args[j] = NULL;     // Đánh dấu vị trí chuyển hướng là NULL
            redirect_input = 1; // Cờ chuyển hướng đầu vào

            if (args[j + 1] == NULL)
            { // Nơi đầu vào được chuyển hướng vào là NULL nghĩa là không có file input.
                printf("Missing input file.\n");
                exit(EXIT_FAILURE); // Báo lỗi xong thoát
            }

            // Tạo file args[i] nếu chưa có; file này sẽ thế chỗ đầu vào cho file STDIN của terminal
            input_fd = open(args[j + 1], O_RDONLY); // input_fd lưu trữ giá trị file descriptor nên phía trên mới khai báo int
            if (input_fd < 0)
            {
                perror("System fail to execuse.");
                exit(EXIT_FAILURE); // Không mở được file nên báo lỗi rồi thoát
            }
            dup2(input_fd, STDIN_FILENO); // Overwrite file STDIN hiện tại bằng file input_fd vừa tạo, luồng đầu vào bây giờ là file input_fd
            close(input_fd);
        }
    }

    // Kiểm tra lỗi
    if (execvp(args[0], args) == -1)
    {
        printf("Command does not exist.\n");
        exit(EXIT_FAILURE);
    }
}

int main()
{
    // Listener để xử lý khi người dùng nhấn Ctrl + C
    signal(SIGINT, sigint_handler);

    // Cấp phát bộ nhớ
    for (int i = 0; i < MAX_LINE / 2 + 1; i++)
    {
        args[i] = malloc(MAX_LINE);
    }

    int should_run = 1; // Biến cờ để xác định chương trình có tiếp tục chạy hay không
    while (should_run)
    {
        printf("it007sh> "); // Dấu nhắc
        fflush(stdout);      // Xóa bộ đệm

        char command[MAX_LINE];
        fgets(command, MAX_LINE, stdin);        // Nhập câu lệnh
        command[strcspn(command, "\n")] = '\0'; // strcspn tìm vị trí đầu tiên có '\n' trong chuỗi; '\0' kết thúc chuỗi tại đó. // Loại bỏ ký tự xuống dòng ('\n') ở cuối chuỗi nhập vào

        // Bỏ qua nếu câu lệnh rỗng
        if (strlen(command) == 0)
            continue;

        // Kiểm tra lệnh đặc biệt của người dùng nhập vào: "exit"
        if (strcmp(command, "exit") == 0)
        {
            should_run = 0; // đổi của hiệu để dừng chuong trình
            continue;
        }

        // Tạo ra tiến trình con để giao nhiệm vụ thực thi lệnh cho nó
        // pid đươc khai báo là biến toàn cục
        pid = fork();
        if (pid == -1)
        {
            perror("fork() failed. Try again.");
            continue;
        }

        // Tiến trình con
        if (pid == 0)
        {

            if (strchr(command, '|'))
            {
                char cmd_list[MAX_COMMAND][MAX_LINE]; // Mảng để chứa các câu lệnh phân tách bằng dấu '|'. Ví dụ ["wc- l", "ls", "cat output.txt"]
                int cmd_count = 0;
                // Khúc này là để tách chuỗi command thành nhiều sub-command bằng dấu '|'
                char *token = strtok(command, "|");
                while (token != NULL && cmd_count < MAX_COMMAND)
                {
                    // xóa khoảng trắng để nhận diện lệnh
                    while (*token == ' ' || *token == '\t')
                    {
                        token++;
                    }

                    int length = strlen(token);
                    while (length > 0 && (token[length - 1] == ' ' || token[length - 1] == '\t'))
                    {
                        token[length - 1] = '\0';
                        length--;
                    }

                    strcpy(cmd_list[cmd_count++], token); // lưu subcommand vào câu lệnh cmd_list
                    token = strtok(NULL, "|");            // tìm subcommand tiếp theo
                }

                // Khúc này là để chạy từng sub-command. Nếu có nhiều hơn 1 sub-command thì cơ chế pipe sẽ có hiệu lực
                int prevfd = STDIN_FILENO;
                for (int i = 0; i < cmd_count; i++)
                {
                    int pipefd[2];
                    if (pipe(pipefd) == -1)
                    {
                        // Xử lý lỗi khi không thể tạo đường ống
                        perror("Pipe fail. Try again");
                        continue;
                    }

                    pid_t cpid = fork(); // Đẻ ra tiến trình con để chạy từng cmd trong trong cmd_list[]
                    if (cpid == -1)
                    {
                        perror("fork() failed in pipeline. Try again.");
                        exit(EXIT_FAILURE);
                    }
                    // Tiến trình con
                    else if (cpid == 0)
                    {
                        close(pipefd[0]);
                        dup2(prevfd, STDIN_FILENO);
                        if (i < cmd_count - 1)
                            dup2(pipefd[1], STDOUT_FILENO); // Chuyển hướng đầu ra của tất cả cmd trừ cmd cuối
                        exec_cmd(cmd_list[i]);
                    }
                    // Tiến trình cha
                    else
                    {
                        close(pipefd[1]);
                        close(prevfd);
                        prevfd = pipefd[0];
                        waitpid(-1, NULL, 0); // Chờ tiến trình con
                    }
                }
                exit(EXIT_SUCCESS);
            }
            else if (strchr(command, ';'))
            {
                char cmd_list[MAX_COMMAND][MAX_LINE]; // Mảng để chứa các câu lệnh phân tách bằng dấu ';'. Ví dụ ["wc- l", "ls", "cat output.txt"]
                int cmd_count = 0;
                // Khúc này là để tách chuỗi command thành nhiều sub-command bằng dấu ';'
                char *token = strtok(command, ";");
                while (token != NULL && cmd_count < MAX_COMMAND)
                {
                    // xóa khoảng trắng để nhận diện lệnh
                    while (*token == ' ' || *token == '\t')
                    {
                        token++;
                    }

                    int length = strlen(token);
                    while (length > 0 && (token[length - 1] == ' ' || token[length - 1] == '\t'))
                    {
                        token[length - 1] = '\0';
                        length--;
                    }

                    strcpy(cmd_list[cmd_count++], token); // lưu subcommand vào câu lệnh cmd_list
                    token = strtok(NULL, ";");            // tìm subcommand tiếp theo
                }

                for (int i = 0; i < cmd_count; i++)
                {
                    pid_t cpid = fork(); // Đẻ ra tiến trình con để chạy từng cmd trong trong cmd_list[]
                    if (cpid == -1)
                    {
                        perror("fork() failed in pipeline. Try again.");
                        exit(EXIT_FAILURE);
                    }
                    // Tiến trình con
                    else if (cpid == 0)
                    {
                        exec_cmd(cmd_list[i]);
                    }
                    // Tiến trình cha
                    else
                    {
                        waitpid(-1, NULL, 0); // Chờ tiến trình con
                    }
                }
                exit(EXIT_SUCCESS);
            }
            else
            {
                pid_t cpid = fork(); // Tạo ra tiến trình con để chạy
                if (cpid == -1)
                {
                    perror("fork() failed in pipeline. Try again.");
                    exit(EXIT_FAILURE);
                }
                // Tiến trình con
                else if (cpid == 0)
                {
                    exec_cmd(command);
                }
                // Tiến trình cha
                else
                {
                    waitpid(-1, NULL, 0); // Chờ tiến trình con
                }
            }
        }

        // Tiến trình cha
        else
        {
            waitpid(-1, NULL, 0); // Chờ tiến trình con thực thi xong
        }
    }

    return 0;
}
