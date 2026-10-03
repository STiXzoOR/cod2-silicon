#include "PC/script/scr_yacc.c"
#include <assert.h>

/* STABS records three pointers then seven 32-bit fields (i386 size 40). */
_Static_assert(offsetof(struct yy_buffer_state, yy_input_file) == 0, "scanner input order");
_Static_assert(offsetof(struct yy_buffer_state, yy_ch_buf) == 8, "scanner char buffer order");
_Static_assert(offsetof(struct yy_buffer_state, yy_buf_pos) == 16, "scanner position order");
_Static_assert(offsetof(struct yy_buffer_state, yy_buf_size) == 24, "scanner scalar field order");
_Static_assert(offsetof(struct yy_buffer_state, yy_buffer_status) == 48, "scanner final field order");
_Static_assert(sizeof(struct yy_buffer_state) == 56, "scanner native allocation size");

YY_BUFFER_STATE yy_current_buffer;
int yy_n_chars;
char *yy_c_buf_p;
char *yytext;
FILE *yyin;
char yy_hold_char;

int main(void)
{
    YY_BUFFER_STATE buffer = yy_create_buffer(NULL, 32);
    assert(buffer->yy_buf_size == 32);
    assert(buffer->yy_buf_pos == buffer->yy_ch_buf);
    assert(buffer->yy_ch_buf[0] == 0 && buffer->yy_ch_buf[1] == 0);
    assert(buffer->yy_is_our_buffer == 1);
    assert(buffer->yy_at_bol == 1);
    assert(buffer->yy_fill_buffer == 1);
    assert(buffer->yy_is_interactive == 0);
    free(buffer->yy_ch_buf);
    free(buffer);
    puts("scanner-buffer");
    return 0;
}
