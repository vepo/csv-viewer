#include "csv-reader.h"
#include "buffer-reader.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

void commit_while_isspace(buffer_reader *reader)
{
    while (buffer_reader_available(reader) > 0 && isspace(buffer_reader_current_char(reader, 0)))
    {
        buffer_reader_commit(reader, 1);
    }
}

int count_spaces_previous(buffer_reader *reader, size_t start_pos)
{
    int spaces = 0;
    while (isspace(buffer_reader_current_char(reader, start_pos - spaces - 1)))
    {
        ++spaces;
    }
    return spaces;
}

void proceed_token(csv_contents *contents, csv_token *current_token, csv_token *last_token)
{
    if (last_token)
    {
        last_token->next = current_token;
        last_token = current_token;
    }
    else
    {
        last_token = contents->first = current_token;
    }
}
size_t process_end_of_token(buffer_reader *reader, size_t current_pos, size_t curr_column, csv_contents *contents)
{
    size_t column = curr_column;
    if (buffer_reader_current_char(reader, current_pos) == '\n')
    {
        contents->lines++;
        contents->columns = MAX(contents->columns, column);
        column = 0;
    }
    else
    {
        ++column;
    }

    buffer_reader_commit(reader, current_pos + 1);
    return column;
}

csv_contents *csv_reader_read_file(char *path)
{
    csv_contents *contents = (csv_contents *)malloc(sizeof(csv_contents));
    contents->lines = 0;
    contents->columns = 0;
    contents->first = NULL;
    csv_token *last_token = NULL;
    buffer_reader *reader = buffer_reader_open(path);

    size_t column = 0;
    bool is_escaped = false;
    while (buffer_reader_acquire(reader))
    {
        while (buffer_reader_has_data(reader))
        {
            commit_while_isspace(reader);
            is_escaped = buffer_reader_current_char(reader, 0) == '"';
            if (is_escaped)
            {
                size_t token_end = 2;
                size_t scaped_counter = 0;
                while ((buffer_reader_available(reader) > token_end + 1))
                {
                    if (buffer_reader_current_char(reader, token_end) == '"' && buffer_reader_current_char(reader, token_end + 1) == '"')
                    {
                        token_end += 2;
                        scaped_counter++;
                        continue;
                    }

                    if (buffer_reader_current_char(reader, token_end) == '"')
                    {
                        ++token_end;
                        break;
                    }
                    else
                    {
                        ++token_end;
                    }
                }
                char *token = (char *)malloc(sizeof(char) * (token_end - scaped_counter));
                scaped_counter = 0;
                bool in_scaped_char = false;
                for (int bufferPos = 1, tokenPos = 0; bufferPos <= token_end; bufferPos++)
                {
                    if (!in_scaped_char && buffer_reader_current_char(reader, bufferPos) == '"')
                    {
                        in_scaped_char = true;
                        scaped_counter++;
                    }
                    else
                    {
                        in_scaped_char = false;
                        token[tokenPos++] = buffer_reader_current_char(reader, bufferPos);
                    }
                }
                token[token_end - scaped_counter - 1] = '\0';
                csv_token *current_token = (csv_token *)malloc(sizeof(csv_token));
                current_token->data = token;
                current_token->x = column;
                current_token->y = contents->lines;
                current_token->next = NULL;

                if (buffer_reader_current_char(reader, token_end) != '\n' && buffer_reader_current_char(reader, token_end) != ';')
                {
                    token_end++;
                }
                while (buffer_reader_available(reader) > token_end && isspace(buffer_reader_current_char(reader, token_end)) && buffer_reader_current_char(reader, token_end) != '\n')
                {
                    ++token_end;
                };
                while (buffer_reader_available(reader) > token_end && buffer_reader_current_char(reader, token_end) != ';' && buffer_reader_current_char(reader, token_end) != '\n' && buffer_reader_current_char(reader, token_end) != EOF)
                {
                    ++token_end;
                };
                column = process_end_of_token(reader, token_end, column, contents);
                proceed_token(contents, current_token, last_token);
                last_token = current_token;
            }
            else
            {
                size_t token_end = 0;
                while ((buffer_reader_available(reader) > token_end || !reader->endReached) && buffer_reader_current_char(reader, token_end) != ';' && buffer_reader_current_char(reader, token_end) != '\n' && buffer_reader_current_char(reader, token_end) != EOF)
                {
                    ++token_end;
                };

                int space_pos = count_spaces_previous(reader, token_end);

                char *token = (char *)malloc(sizeof(char) * (token_end + 1 - space_pos));
                buffer_reader_current_copy(reader, token_end + 1 - space_pos, token);
                csv_token *current_token = (csv_token *)malloc(sizeof(csv_token));
                current_token->data = token;
                current_token->x = column;
                current_token->y = contents->lines;
                current_token->next = NULL;
                column = process_end_of_token(reader, token_end, column, contents);
                proceed_token(contents, current_token, last_token);
                last_token = current_token;
            }
        }
    }

    if (contents->first)
    {
        contents->columns++;
        contents->lines++;
    }

    buffer_reader_release(reader);
    return contents;
}

csv_token *csv_reader_get_token(size_t x, size_t y, const csv_contents *contents)
{
    csv_token *curr_token = contents->first;
    while (curr_token)
    {
        if (curr_token->x == x && curr_token->y == y)
        {
            return curr_token;
        }
        curr_token = curr_token->next;
    }
    return NULL;
}

void csv_contents_dispose(csv_contents *contents)
{
    if (contents)
    {
        if (contents->first)
        {
            csv_token_dispose(contents->first);
        }
        free(contents);
    }
}

void csv_token_dispose(csv_token *token)
{
    if (token)
    {
        if (token->next)
        {
            csv_token_dispose(token->next);
        }

        if (token->data)
        {
            free(token->data);
        }
        free(token);
    }
}