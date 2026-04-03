#include <yed/plugin.h>
#include <yed/syntax.h>

static yed_syntax syn;

#define _CHECK(x, r)                                                      \
do {                                                                      \
    if (x) {                                                              \
        LOG_FN_ENTER();                                                   \
        yed_log("[!] " __FILE__ ":%d regex error for '%s': %s", __LINE__, \
                r,                                                        \
                yed_syntax_get_regex_err(&syn));                          \
        LOG_EXIT();                                                       \
    }                                                                     \
} while (0)

#define SYN()          yed_syntax_start(&syn)
#define ENDSYN()       yed_syntax_end(&syn)
#define APUSH(s)       yed_syntax_attr_push(&syn, s)
#define APOP(s)        yed_syntax_attr_pop(&syn)
#define RANGE(r)       _CHECK(yed_syntax_range_start(&syn, r), r)
#define ONELINE()      yed_syntax_range_one_line(&syn)
#define SKIP(r)        _CHECK(yed_syntax_range_skip(&syn, r), r)
#define ENDRANGE(r)    _CHECK(yed_syntax_range_end(&syn, r), r)
#define REGEX(r)       _CHECK(yed_syntax_regex(&syn, r), r)
#define REGEXSUB(r, g) _CHECK(yed_syntax_regex_sub(&syn, r, g), r)
#define KWD(k)         yed_syntax_kwd(&syn, k)

#ifdef __APPLE__
#define WB "[[:>:]]"
#else
#define WB "\\b"
#endif

void estyle(yed_event *event)   { yed_syntax_style_event(&syn, event);         }
void ebuffdel(yed_event *event) { yed_syntax_buffer_delete_event(&syn, event); }
void ebuffmod(yed_event *event) { yed_syntax_buffer_mod_event(&syn, event);    }
void eline(yed_event *event)  {
    yed_frame *frame;

    frame = event->frame;

    if (!frame
    ||  !frame->buffer
    ||  frame->buffer->kind != BUFF_KIND_FILE
    ||  frame->buffer->ft != yed_get_ft("Man")) {
        return;
    }

    yed_syntax_line_event(&syn, event);
}


void unload(yed_plugin *self) {
    yed_syntax_free(&syn);
}

int yed_plugin_boot(yed_plugin *self) {
    yed_event_handler style;
    yed_event_handler buffdel;
    yed_event_handler buffmod;
    yed_event_handler line;


    YED_PLUG_VERSION_CHECK();

    yed_plugin_set_unload_fn(self, unload);

    style.kind = EVENT_STYLE_CHANGE;
    style.fn   = estyle;
    yed_plugin_add_event_handler(self, style);

    buffdel.kind = EVENT_BUFFER_PRE_DELETE;
    buffdel.fn   = ebuffdel;
    yed_plugin_add_event_handler(self, buffdel);

    buffmod.kind = EVENT_BUFFER_POST_MOD;
    buffmod.fn   = ebuffmod;
    yed_plugin_add_event_handler(self, buffmod);

    line.kind = EVENT_LINE_PRE_DRAW;
    line.fn   = eline;
    yed_plugin_add_event_handler(self, line);


    SYN();
        /* Comments: .\" or .\" or .\" style */
        APUSH("&code-comment");
            RANGE("^\\.\\\\\"");
                ONELINE();
            ENDRANGE("$");
            RANGE("^\\.\\.\\\\\"");
                ONELINE();
            ENDRANGE("$");
            RANGE("^'\\\\\"");
                ONELINE();
            ENDRANGE("$");
        APOP();

        /* Strings: quoted text */
        APUSH("&code-string");
            RANGE("\""); SKIP("\\\\\""); ENDRANGE("\"");
        APOP();

        /* Section headers with arguments: .SH/.SS base color, argument overridden */
        APUSH("&code-preprocessor");
            RANGE("^\\.(SH|SS)[[:space:]]");
                ONELINE();
                APUSH("&code-fn-call");
                    REGEXSUB("(.+)", 1);
                APOP();
            ENDRANGE("$");
        APOP();

        /* Section headers: .TH, .SH, .SS (without arguments) */
        APUSH("&code-preprocessor");
            REGEXSUB("^(\\.(TH|SH|SS))"WB, 1);
        APOP();

        /* Font/style macros: .B, .I, .BI, .BR, .IB, .IR, .RB, .RI, .SM, .SB */
        APUSH("&code-keyword");
            REGEXSUB("^(\\.(B|I|BI|BR|IB|IR|RB|RI|SM|SB))"WB, 1);
        APOP();

        /* Paragraph and formatting macros */
        APUSH("&code-control-flow");
            REGEXSUB("^(\\.(PP|LP|P|TP|IP|HP|RS|RE|nf|fi|sp|ne|na|ad|ce|in|ti|ta))"WB, 1);
            REGEXSUB("^(\\.(br|bp|ll|pl|nh|hy|ft|ps|vs|mk|rt))"WB, 1);
        APOP();

        /* Conditional macros */
        APUSH("&code-control-flow");
            REGEXSUB("^(\\.(if|ie|el|while))"WB, 1);
        APOP();

        /* String/macro definitions and register operations */
        APUSH("&code-preprocessor");
            REGEXSUB("^(\\.(de|ds|nr|rr|rm|am))"WB, 1);
        APOP();

        /* Diversions */
        APUSH("&code-preprocessor");
            REGEXSUB("^(\\.(di|da|dt))"WB, 1);
        APOP();

        /* Source/include */
        APUSH("&code-preprocessor");
            REGEXSUB("^(\\.(so|mso))"WB, 1);
        APOP();

        /* Man-specific macros: .TQ, .EX, .EE, .SY, .YS, .OP, .MT, .ME, .UR, .UE */
        APUSH("&code-fn-call");
            REGEXSUB("^(\\.(TQ|EX|EE|SY|YS|OP|MT|ME|UR|UE))"WB, 1);
        APOP();

        /* Table/equation macros */
        APUSH("&code-fn-call");
            REGEXSUB("^(\\.(TS|TE|T&|EQ|EN))"WB, 1);
        APOP();

        /* Numbers */
        APUSH("&code-number");
            REGEXSUB("(^|[^[:alnum:]_])(-?[[:digit:]]+)"WB, 2);
        APOP();

        /* Escape sequences: \fB, \fI, \fR, \fP, \(, \*, etc. */
        APUSH("&code-typename");
            REGEXSUB("(\\\\f[BIRP])", 1);
            REGEXSUB("(\\\\\\([a-zA-Z]{2})", 1);
            REGEXSUB("(\\\\\\*\\([a-zA-Z]{2})", 1);
            REGEXSUB("(\\\\\\*\\[[a-zA-Z_][a-zA-Z0-9_]*\\])", 1);
            REGEXSUB("(\\\\\\*[a-zA-Z])", 1);
            REGEXSUB("(\\\\n\\[[a-zA-Z_][a-zA-Z0-9_]*\\])", 1);
            REGEXSUB("(\\\\n[a-zA-Z])", 1);
            REGEXSUB("(\\\\[enst&|^0dDwu])", 1);
        APOP();
    ENDSYN();

    return 0;
}
