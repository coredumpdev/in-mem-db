#ifndef ENUMS_H
#define ENUMS_H

typedef enum {
    NOT_FOUND,
    FOUND

}ACTIONS;

typedef enum {
    FILE_SUCCESS,
    FILE_NOT_FOUND,
    NOT_OPEN,
    READ_ERROR
}File;

typedef enum {
    TCP_SUCCESS = 0,
    TCP_DESC_ERR = -1,
    TCP_SOPT_ERR = -2,
    TCP_BIND_ERR = -3,
    TCP_LIST_ERR = -4


}TCPSocket;


#endif
