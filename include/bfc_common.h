/**
 * @file bfc_common.h
 * @brief Common compile-time utility macros.
 *
 * @details
 * Contains small utilities shared across otherwise independent compiler modules.
 */
#ifndef BFC_COMMON_H
#define BFC_COMMON_H

/**
 * @brief Returns the number of elements in an actual array.
 *
 * @note The argument must be an array expression, not a pointer.
 */
#define BFC_ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

#endif  // BFC_COMMON_H
