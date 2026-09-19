#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/entities.h"

int main() {
    struct Counter c = {1, 1, 1, 1, 1, 1, 1};

    int fd = open("data/counter.dat", O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        perror("open failed");
        return 1;
    }

    ssize_t written = write(fd, &c, sizeof(struct Counter));
    if (written != sizeof(struct Counter)) {
        perror("write failed");
        close(fd);
        return 1;
    }
    printf("Wrote counter: %d %d %d %d %d %d %d\n",
           c.next_customer_id, c.next_employee_id, c.next_manager_id,
           c.next_admin_id, c.next_loan_id, c.next_feedback_id, c.next_transaction_id);

    lseek(fd, 0, SEEK_SET);
    struct Counter c2;
    ssize_t r = read(fd, &c2, sizeof(struct Counter));
    if (r != sizeof(struct Counter)) {
        perror("read failed");
        close(fd);
        return 1;
    }
    printf("Read back:   %d %d %d %d %d %d %d\n",
           c2.next_customer_id, c2.next_employee_id, c2.next_manager_id,
           c2.next_admin_id, c2.next_loan_id, c2.next_feedback_id, c2.next_transaction_id);

    close(fd);
    return 0;
}
