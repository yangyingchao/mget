#ifndef _DATA_UTLIS_H_
#define _DATA_UTLIS_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

typedef void (*free_func)   (void*);

typedef struct _mget_slis
{
    void*     data;
    free_func f;
    struct _mget_slis* next;
}mget_slis;

mget_slis* mget_slis_append(mget_slis* l, void*data, free_func f);
// #ifdef __cplusplus
// }
// #endif

#endif /* _DATA_UTLIS_H_ */
