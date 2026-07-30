/*
 * Source-file loading and source-line access.
 *
 * bfc_program_t owns a copied path and a null-terminated source buffer.
 * bfc_program_getline() returns a separate allocation owned by its caller.
 */

#include "bfc_io.h"
#include "bfc_error.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bfc_memory.h"

/*
 * Load an entire source file into memory and transfer ownership through
 * *program only after every allocation and read succeeds.
 */
bfc_error_t bfc_program_create(bfc_program_t** program, char const* file_path)
{
    FILE* file_handle;

    if ((file_handle = fopen(file_path, "rb")))
    {
        bfc_program_t* prog = nullptr;
        prog                = BFC_CALLOC_ARRAY(prog, 1);
        if (!prog)
        {
            fclose(file_handle);

            return BFC_ERR_ALLOC;
        }

        int seek_status = fseek(file_handle, 0, SEEK_END);
        if (seek_status != 0)
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Unable to seek the end of file!");
        }

        long file_size = ftell(file_handle);
        if (file_size == -1L)
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Unable to perform ftell on file!");
        }

        seek_status = fseek(file_handle, 0, SEEK_SET);
        if (seek_status != 0)
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Unable to seek the start of file!");
        }

        /*
         * Validate conversion from ftell()'s signed long to size_t and reserve
         * one additional byte for the null terminator.
         */
        if (file_size < 0 || (uintmax_t) file_size > (uintmax_t) (SIZE_MAX - 1))
        {
            free(prog);
            fclose(file_handle);

            return bfc_make_error(ERR_IO, "Invalid file size!");
        }

        const size_t file_size_bytes = (size_t) file_size;

        prog->buffer = malloc(file_size_bytes + 1);
        if (!prog->buffer)
        {
            free(prog);
            fclose(file_handle);

            return BFC_ERR_ALLOC;
        }

        prog->file_size = file_size_bytes;

        prog->path = strdup(file_path);

        if (!prog->path)
        {
            free(prog->buffer);
            free(prog);
            fclose(file_handle);

            return BFC_ERR_ALLOC;
        }

        size_t end = fread(prog->buffer, sizeof(char), prog->file_size, file_handle);
        if (ferror(file_handle) != 0 || end != (size_t) prog->file_size)
        {
            free(prog->path);
            free(prog->buffer);
            free(prog);
            fclose(file_handle);

            return bfc_make_errorf(ERR_IO, "Unable to read from file '%s'!", file_path);
        }

        prog->buffer[end] = '\0';

        fclose(file_handle);

        /*
         * Count logical lines. A nonempty final line counts even when the file
         * does not end with a newline.
         */
        prog->line_count = 0;

        for (size_t i = 0; i < prog->file_size; ++i)
        {
            if (prog->buffer[i] == '\n')
            {
                ++prog->line_count;
            }
        }

        if (prog->file_size > 0 && prog->buffer[prog->file_size - 1] != '\n')
        {
            ++prog->line_count;
        }

        *program = prog;
        return BFC_ERR_OK;
    }

    return bfc_make_errorf(ERR_IO, "No such file or directory: '%s'", file_path);
}

/* Release both owned strings and then the program object itself. */
void bfc_program_destroy(bfc_program_t** pprogram)
{
    if (!pprogram || !*pprogram)
    {
        return;
    }

    free((*pprogram)->path);
    free((*pprogram)->buffer);
    free(*pprogram);

    *pprogram = nullptr;
}

/*
 * Return a borrowed pointer to the final path component; no allocation occurs.
 * Both POSIX and Windows path separators are recognized.
 */
char const* bfc_program_getname(bfc_program_t const* program)
{
    char const* name = program->path;

    for (char const* p = program->path; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\')
        {
            name = p + 1;
        }
    }

    return name;
}

/*
 * Return a newly allocated copy of a one-based source line. The caller owns
 * the result and must free it.
 */
char* bfc_program_getline(bfc_program_t const* const program, size_t const n)
{
    if (n == 0 || n > program->line_count)
    {
        return nullptr;
    }

    size_t      current_line = 1;
    char const* start        = program->buffer;
    char const* end          = program->buffer;

    while (current_line < n)
    {
        end = strchr(start, '\n');
        if (end == nullptr)
        {
            return nullptr;
        }

        start = end + 1;
        ++current_line;
    }

    end = strchr(start, '\n');

    const size_t line_len = end ? (size_t) (end - start) : strlen(start);

    char* line_buf = malloc(line_len + 1);

    if (!line_buf)
    {
        return nullptr;
    }

    memcpy(line_buf, start, line_len);
    line_buf[line_len] = '\0';

    return line_buf;
}
