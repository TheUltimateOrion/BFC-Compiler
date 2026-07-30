#ifndef BFC_COMMON_H
#define BFC_COMMON_H

/*
 * Returns the number of elements in an actual array.
 *
 * This macro must not be used with a pointer because sizeof(pointer) does not
 * describe the size of the pointed-to allocation.
 */
#define BFC_ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

#endif  // BFC_COMMON_H
