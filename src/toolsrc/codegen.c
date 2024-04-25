#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "plasm.h"
/*
 * Symbol table and fixup information.
 */
#define ID_LEN    32
static int  consts    = 0;
static int  externs   = 0;
static int  globals   = 0;
static int  locals    = 0;
static int  localsize = 0;
static int  predefs   = 0;
static int  defs      = 0;
static int  asmdefs   = 0;
static int  codetags  = 1; // Fix check for break_tag and cont_tag
static int  fixups    = 0;
static int  lastglobalsize = 0;
static char idconst_name[1024][ID_LEN+1];
static int  idconst_value[1024];
static char idglobal_name[1024][ID_LEN+1];
static int  idglobal_type[1024];
static int  idglobal_tag[1024];
static char idlocal_name[128][ID_LEN+1];
static int  idlocal_type[128];
static int  idlocal_offset[128];
static char fixup_size[2048];
static int  fixup_type[2048];
static int  fixup_tag[2048];
static int  savelocalsize = 0;
static int  savelocals    = 0;
static char savelocal_name[128][ID_LEN+1];
static int  savelocal_type[128];
static int  savelocal_offset[128];
static t_opseq optbl[2048];
static t_opseq *freeop_lst = &optbl[0];
static t_opseq *pending_seq = 0;
#define FIXUP_BYTE    0x00
#define FIXUP_WORD    0x80
int id_match(char *name, int len, char *id)
{
    if (len > ID_LEN) len = ID_LEN;
    if (len == id[0])
    {
        while (len--)
        {
            if (toupper(name[len]) != id[1 + len])
                return (0);
        }
        return (1);
    }
    return (0);
}
int idconst_lookup(char *name, int len)
{
    int i;
    for (i = 0; i < consts; i++)
        if (id_match(name, len, &(idconst_name[i][0])))
            return (i);
    return (-1);
}
int idlocal_lookup(char *name, int len)
{
    int i;
    for (i = 0; i < locals; i++)
        if (id_match(name, len, &(idlocal_name[i][0])))
            return (i);
    return (-1);
}
int idglobal_lookup(char *name, int len)
{
    int i;
    for (i = 0; i < globals; i++)
        if (id_match(name, len, &(idglobal_name[i][0])))
        {
            if (idglobal_type[i] & EXTERN_TYPE)
                idglobal_type[i] |= ACCESSED_TYPE;
            return (i);
        }
    return (-1);
}
int idconst_add(char *name, int len, int value)
{
    char c;
    if (consts > 1024)
    {
        printf("Constant count overflow\n");
        return (0);
    }
    if (idconst_lookup(name, len) > 0)
    {
        parse_error("const/global name conflict\n");
        return (0);
    }
    if (idglobal_lookup(name, len) > 0)
    {
        parse_error("global label already defined\n");
        return (0);
    }
    if (len > ID_LEN) len = ID_LEN;
    c = name[len];
    name[len] = '\0';
    emit_idconst(name, value);
    name[len] = c;
    idconst_name[consts][0] = len;
    while (len--)
        idconst_name[consts][1 + len] = toupper(name[len]);
    idconst_value[consts] = value;
    consts++;
    return (1);
}
int idlocal_add(char *name, int len, int type, int size)
{
    char c;
    if (localsize > 255)
    {
        printf("Local variable size overflow\n");
        return (0);
    }
    if (idconst_lookup(name, len) > 0)
    {
        parse_error("const/local name conflict\n");
        return (0);
    }
    if (idlocal_lookup(name, len) > 0)
    {
        parse_error("local label already defined\n");
        return (0);
    }
    if (len > ID_LEN) len = ID_LEN;
    c = name[len];
    name[len] = '\0';
    emit_idlocal(name, localsize);
    name[len] = c;
    idlocal_name[locals][0] = len;
    while (len--)
        idlocal_name[locals][1 + len] = toupper(name[len]);
    idlocal_type[locals]   = type | LOCAL_TYPE;
    idlocal_offset[locals] = localsize;
    localsize += size;
    locals++;
    return (1);
}
int idglobal_add(char *name, int len, int type, int size)
{
    if (globals > 1024)
    {
        printf("Global variable count overflow\n");
        return (0);
    }
    if (idconst_lookup(name, len) > 0)
    {
        parse_error("const/global name conflict\n");
        return (0);
    }
    if (idglobal_lookup(name, len) > 0)
    {
        parse_error("global label already defined\n");
        return (0);
    }
    if (len > ID_LEN) len = ID_LEN;
    idglobal_name[globals][0] = len;
    while (len--)
        idglobal_name[globals][1 + len] = toupper(name[len]);
    idglobal_type[globals] = type;
    if (!(type & EXTERN_TYPE))
    {
        emit_idglobal(globals, size, name);
        idglobal_tag[globals] = globals;
        globals++;
    }
    else
    {
        fprintf(outputfile, "\t\t\t\t\t; %s -> X%03d\n", &idglobal_name[globals][1], externs);
        idglobal_tag[globals++] = externs++;
    }
    return (1);
}
int id_add(char *name, int len, int type, int size)
{
    return ((type & LOCAL_TYPE) ? idlocal_add(name, len, type, size) : idglobal_add(name, len, type, size));
}
void idlocal_reset(void)
{
    locals    = 0;
    localsize = 0;
}
void idlocal_save(void)
{
    savelocals    = locals;
    savelocalsize = localsize;
    memcpy(savelocal_name,   idlocal_name,   locals*(ID_LEN+1));
    memcpy(savelocal_type,   idlocal_type,   locals*sizeof(int));
    memcpy(savelocal_offset, idlocal_offset, locals*sizeof(int));
    locals    = 0;
    localsize = 0;
}
void idlocal_restore(void)
{
    locals    = savelocals;
    localsize = savelocalsize;
    memcpy(idlocal_name,   savelocal_name,   locals*(ID_LEN+1));
    memcpy(idlocal_type,   savelocal_type,   locals*sizeof(int));
    memcpy(idlocal_offset, savelocal_offset, locals*sizeof(int));
}
int idfunc_add(char *name, int len, int type, int tag)
{
    if (globals > 1024)
    {
        printf("Global variable count overflow\n");
        return (0);
    }
    if (len > ID_LEN) len = ID_LEN;
    idglobal_name[globals][0] = len;
    while (len--)
        idglobal_name[globals][1 + len] = toupper(name[len]);
    idglobal_type[globals]  = type;
    idglobal_tag[globals++] = tag;
    if (type & EXTERN_TYPE)
        fprintf(outputfile, "\t\t\t\t\t; %s -> X%03d\n", &idglobal_name[globals - 1][1], tag);
    return (1);
}
int idfunc_set(char *name, int len, int type, int tag)
{
    int i;
    if (((i = idglobal_lookup(name, len)) >= 0) && (idglobal_type[i] & FUNC_TYPE))
    {
        idglobal_tag[i]  = tag;
        idglobal_type[i] = type;
        return (type);
    }
    parse_error("Undeclared identifier");
    return (0);
}
void idglobal_size(int type, int size, int constsize)
{
    if (size > constsize)
        emit_data(0, 0, 0, size - constsize);
    else if (size)
        emit_data(0, 0, 0, size);
}
void idlocal_size(int size)
{
    localsize += size;
    if (localsize > 255)
    {
        parse_error("Local variable size overflow\n");
    }
}
int id_tag(char *name, int len)
{
    int i;
    if ((i = idlocal_lookup(name, len)) >= 0)
        return (idlocal_offset[i]);
    if ((i = idglobal_lookup(name, len)) >= 0)
        return (idglobal_tag[i]);
    return (-1);
}
int id_const(char *name, int len)
{
    int i;
    if ((i = idconst_lookup(name, len)) >= 0)
        return (idconst_value[i]);
    parse_error("Undeclared constant");
    return (0);
}
int id_type(char *name, int len)
{
    int i;
    if ((i = idconst_lookup(name, len)) >= 0)
        return (CONST_TYPE);
    if ((i = idlocal_lookup(name, len)) >= 0)
        return (idlocal_type[i] | LOCAL_TYPE);
    if ((i = idglobal_lookup(name, len)) >= 0)
        return (idglobal_type[i]);
    parse_error("Undeclared identifier");
    return (0);
}
int tag_new(int type)
{
    if (type & EXTERN_TYPE)
    {
        if (externs > 254)
            parse_error("External variable count overflow\n");
        return (externs++);
    }
    if (type & PREDEF_TYPE)
        return (predefs++);
    if (type & ASM_TYPE)
        return (asmdefs++);
    if (type & DEF_TYPE)
        return (defs++);
    if (type & BRANCH_TYPE)
        return (codetags++);
    return globals++;
}
int fixup_new(int tag, int type, int size)
{
    fixup_tag[fixups]  = tag;
    fixup_type[fixups] = type;
    fixup_size[fixups] = size;
    return (fixups++);
}
/*
 * Emit assembly code.
 */
static const char *DB = ".BYTE";
static const char *DW = ".WORD";
static const char *DS = ".RES";
static char LBL = (char) ':';
char *supper(char *s)
{
    static char su[80];
    int i;
    for (i = 0; s[i]; i++)
        su[i] = toupper(s[i]);
    su[i] = '\0';
    return su;
}
char *tag_string(int tag, int type)
{
    static char str[16];
    char t;

    if (type & EXTERN_TYPE)
        t = 'X';
    else if (type & DEF_TYPE)
        t = 'C';
    else if (type & ASM_TYPE)
        t = 'A';
    else if (type & BRANCH_TYPE)
        t = 'B';
    else if (type & PREDEF_TYPE)
        t = 'P';
    else
        t = 'D';
    sprintf(str, "_%c%03d", t, tag);
    return str;
}
void emit_dci(char *str, int len)
{
    if (len--)
    {
        fprintf(outputfile, "\t; DCI STRING: %s\n", supper(str));
        fprintf(outputfile, "\t%s\t$%02X", DB, toupper(*str++) | (len ? 0x80 : 0x00));
        while (len--)
            fprintf(outputfile, ",$%02X", toupper(*str++) | (len ? 0x80 : 0x00));
        fprintf(outputfile, "\n");
    }
}
void emit_flags(int flags)
{
    if (flags & ACME)
    {
        DB = "!BYTE";
        DW = "!WORD";
        DS = "!FILL";
        LBL = ' ';
    }
}
void emit_header(void)
{
    int i;

    if (outflags & ACME)
        fprintf(outputfile, "; ACME COMPATIBLE OUTPUT\n");
    else
        fprintf(outputfile, "; CA65 COMPATIBLE OUTPUT\n");
    if (outflags & MODULE)
    {
        fprintf(outputfile, "\t%s\t_SEGEND-_SEGBEGIN\t; LENGTH OF HEADER + CODE/DATA + BYTECODE SEGMENT\n", DW);
        fprintf(outputfile, "_SEGBEGIN%c\n", LBL);
        fprintf(outputfile, "\t%s\t$6502\t\t\t; MAGIC #\n", DW);
        fprintf(outputfile, "\t%s\t_SYSFLAGS\t\t\t; SYSTEM FLAGS\n", DW);
        fprintf(outputfile, "\t%s\t_SUBSEG\t\t\t; BYTECODE SUB-SEGMENT\n", DW);
        fprintf(outputfile, "\t%s\t_DEFCNT\t\t\t; BYTECODE DEF COUNT\n", DW);
        fprintf(outputfile, "\t%s\t_INIT\t\t\t; MODULE INITIALIZATION ROUTINE\n", DW);
    }
    else
    {
        fprintf(outputfile, "\tJMP\t_INIT\t\t\t; MODULE INITIALIZATION ROUTINE\n");
    }
    /*
     * Init free op sequence table
     */
    for (i = 0; i < sizeof(optbl)/sizeof(t_opseq)-1; i++)
        optbl[i].nextop = &optbl[i+1];
    optbl[i].nextop = NULL;
}
void emit_rld(void)
{
    int i, j;

    fprintf(outputfile, ";\n; RE-LOCATEABLE DICTIONARY\n;\n");
    /*
     * First emit the bytecode definition entrypoint information.
     */
    /*
    for (i = 0; i < globals; i++)
        if (!(idglobal_type[i] & EXTERN_TYPE) && (idglobal_type[i] & DEF_TYPE))
        {
            printf("\t%s\t$02\t\t\t; CODE TABLE FIXUP\n", DB);
            printf("\t%s\t_C%03d\t\t\n", DW, idglobal_tag[i]);
            printf("\t%s\t$00\n", DB);
        }
    */
    j = outflags & INIT ? defs - 1 : defs;
    for (i = 0; i < j; i++)
    {
        fprintf(outputfile, "\t%s\t$02\t\t\t; CODE TABLE FIXUP\n", DB);
        fprintf(outputfile, "\t%s\t_C%03d\t\t\n", DW, i);
        fprintf(outputfile, "\t%s\t$00\n", DB);
    }
    /*
     * Now emit the fixup table.
     */
    for (i = 0; i < fixups; i++)
    {
        if (fixup_type[i] & EXTERN_TYPE)
        {
            fprintf(outputfile, "\t%s\t$%02X\t\t\t; EXTERNAL FIXUP\n", DB, 0x11 + fixup_size[i] & 0xFF);
            fprintf(outputfile, "\t%s\t_F%03d-_SEGBEGIN\t\t\n", DW, i);
            fprintf(outputfile, "\t%s\t%d\t\t\t; ESD INDEX\n", DB, fixup_tag[i]);
        }
        else
        {
            fprintf(outputfile, "\t%s\t$%02X\t\t\t; INTERNAL FIXUP\n", DB, 0x01 + fixup_size[i] & 0xFF);
            fprintf(outputfile, "\t%s\t_F%03d-_SEGBEGIN\t\t\n", DW, i);
            fprintf(outputfile, "\t%s\t$00\n", DB);
        }
    }
    fprintf(outputfile, "\t%s\t$00\t\t\t; END OF RLD\n", DB);
}
void emit_esd(void)
{
    int i;

    fprintf(outputfile, ";\n; EXTERNAL/ENTRY SYMBOL DICTIONARY\n;\n");
    for (i = 0; i < globals; i++)
    {
        if (idglobal_type[i] & ACCESSED_TYPE) // Only refer to accessed externals
        {
            emit_dci(&idglobal_name[i][1], idglobal_name[i][0]);
            fprintf(outputfile, "\t%s\t$10\t\t\t; EXTERNAL SYMBOL FLAG\n", DB);
            fprintf(outputfile, "\t%s\t%d\t\t\t; ESD INDEX\n", DW, idglobal_tag[i]);
        }
        else if (idglobal_type[i] & EXPORT_TYPE)
        {
            emit_dci(&idglobal_name[i][1], idglobal_name[i][0]);
            fprintf(outputfile, "\t%s\t$08\t\t\t; ENTRY SYMBOL FLAG\n", DB);
            fprintf(outputfile, "\t%s\t%s\t\t\n", DW, tag_string(idglobal_tag[i], idglobal_type[i]));
        }
    }
    fprintf(outputfile, "\t%s\t$00\t\t\t; END OF ESD\n", DB);
}
void emit_trailer(void)
{
    if (!(outflags & BYTECODE_SEG))
        emit_bytecode_seg();
    if (!(outflags & INIT))
        fprintf(outputfile, "_INIT\t=\t0\n");
    if (!(outflags & SYSFLAGS))
        fprintf(outputfile, "_SYSFLAGS\t=\t0\n");
    if (outflags & MODULE)
    {
        fprintf(outputfile, "_DEFCNT\t=\t%d\n", defs);
        fprintf(outputfile, "_SEGEND%c\n", LBL);
        emit_rld();
        emit_esd();
    }
}
void emit_moddep(char *name, int len)
{
    if (outflags & MODULE)
    {
        if (name)
        {
            emit_dci(name, len);
            idglobal_add(name, len, EXTERN_TYPE | WORD_TYPE, 2); // Add to symbol table
        }
        else
            fprintf(outputfile, "\t%s\t$00\t\t\t; END OF MODULE DEPENDENCIES\n", DB);
    }
}
void emit_sysflags(int val)
{
    fprintf(outputfile, "_SYSFLAGS\t=\t$%04X\t\t; SYSTEM FLAGS\n", val);
    outflags |= SYSFLAGS;
}
void emit_bytecode_seg(void)
{
    if ((outflags & MODULE) && !(outflags & BYTECODE_SEG))
    {
        if (lastglobalsize == 0) // Pad a byte if last label is at end of data segment
            fprintf(outputfile, "\t%s\t$00\t\t\t; PAD BYTE\n", DB);
        fprintf(outputfile, "_SUBSEG%c\t\t\t\t; BYTECODE STARTS\n", LBL);
    }
    outflags |= BYTECODE_SEG;
}
void emit_comment(char *s)
{
    fprintf(outputfile, "\t\t\t\t\t; %s\n", s);
}
void emit_asm(char *s)
{
    fprintf(outputfile, "%s\n", s);
}
void emit_idlocal(char *name, int value)
{
    fprintf(outputfile, "\t\t\t\t\t; %s -> [%d]\n", name, value);
}
void emit_idglobal(int tag, int size, char *name)
{
    lastglobalsize = size;
    if (size == 0)
        fprintf(outputfile, "_D%03d%c\t\t\t\t\t; %s\n", tag, LBL, name);
    else
        fprintf(outputfile, "_D%03d%c\t%s\t%d\t\t\t; %s\n", tag, LBL, DS, size, name);
}
void emit_idfunc(int tag, int type, char *name, int is_bytecode)
{
    if (name)
        fprintf(outputfile, "%s%c\t\t\t\t\t; %s()\n", tag_string(tag, type), LBL, name);
    if (!(outflags & MODULE))
    {
        //fprintf(outputfile, "%s%c\n", name, LBL);
        if (is_bytecode)
            fprintf(outputfile, "\tJSR\tINTERP\n");
    }
}
void emit_lambdafunc(int tag, char *name, int cparams, t_opseq *lambda_seq)
{
    emit_idfunc(tag, DEF_TYPE, name, 1);
    if (cparams)
        fprintf(outputfile, "\t%s\t$58,$%02X,$%02X\t\t; ENTER\t%d,%d\n", DB, cparams*2, cparams, cparams*2, cparams);
    emit_seq(lambda_seq);
    emit_pending_seq();
    if (cparams)
        fprintf(outputfile, "\t%s\t$5A,$%02X\t\t\t; LEAVE\t%d\n", DB, cparams*2, cparams*2);
    else
        fprintf(outputfile, "\t%s\t$5C\t\t\t; RET\n", DB);
}
void emit_idconst(char *name, int value)
{
    fprintf(outputfile, "\t\t\t\t\t; %s = %d\n", name, value);
}
int emit_data(int vartype, int consttype, long constval, int constsize)
{
    int datasize, i;
    unsigned char *str;
    if (consttype == 0)
    {
        datasize = constsize;
        fprintf(outputfile, "\t%s\t$%02X\n", DS, constsize);
    }
    else if (consttype & STRING_TYPE)
    {
        str = (unsigned char *)constval;
        constsize = *str++;
        datasize = constsize + 1;
        fprintf(outputfile, "\t%s\t$%02X\n", DB, constsize);
        while (constsize-- > 0)
        {
            fprintf(outputfile, "\t%s\t$%02X", DB, *str++);
            for (i = 0; i < 7; i++)
            {
                if (constsize-- > 0)
                    fprintf(outputfile, ",$%02X", *str++);
                else
                    break;
            }
            fprintf(outputfile, "\n");
        }
    }
    else if (consttype & ADDR_TYPE)
    {
        if (vartype & WORD_TYPE)
        {
            int fixup = fixup_new(constval, consttype, FIXUP_WORD);
            datasize = 2;
            if (consttype & EXTERN_TYPE)
                fprintf(outputfile, "_F%03d%c\t%s\t0\t\t\t; %s\n", fixup, LBL, DW, tag_string(constval, consttype));
            else
                fprintf(outputfile, "_F%03d%c\t%s\t%s\n", fixup, LBL, DW, tag_string(constval, consttype));
        }
        else
        {
            int fixup = fixup_new(constval, consttype, FIXUP_BYTE);
            datasize = 1;
            if (consttype & EXTERN_TYPE)
                fprintf(outputfile, "_F%03d%c\t%s\t0\t\t\t; %s\n", fixup, LBL, DB, tag_string(constval, consttype));
            else
                fprintf(outputfile, "_F%03d%c\t%s\t%s\n", fixup, LBL, DB, tag_string(constval, consttype));
        }
    }
    else
    {
        if (vartype & WORD_TYPE)
        {
            datasize = 2;
            fprintf(outputfile, "\t%s\t$%04lX\n", DW, constval & 0xFFFF);
        }
        else
        {
            datasize = 1;
            fprintf(outputfile, "\t%s\t$%02lX\n", DB, constval & 0xFF);
        }
    }
    return (datasize);
}
void emit_codetag(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "_B%03d%c\n", tag, LBL);
}
void emit_const(int cval)
{
    emit_pending_seq();
    if ((cval & 0xFFFF) == 0xFFFF)
        fprintf(outputfile, "\t%s\t$20\t\t\t; MINUS ONE\n", DB);
    else if ((cval & 0xFFF0) == 0x0000)
        fprintf(outputfile, "\t%s\t$%02X\t\t\t; CN\t%d\n", DB, cval*2, cval);
    else if ((cval & 0xFF00) == 0x0000)
        fprintf(outputfile, "\t%s\t$2A,$%02X\t\t\t; CB\t%d\n", DB, cval, cval);
    else if ((cval & 0xFF00) == 0xFF00)
        fprintf(outputfile, "\t%s\t$5E,$%02X\t\t\t; CFFB\t%d\n", DB, cval&0xFF, cval);
    else
        fprintf(outputfile, "\t%s\t$2C,$%02X,$%02X\t\t; CW\t%d\n", DB, cval&0xFF,(cval>>8)&0xFF, cval);
}
void emit_conststr(long conststr)
{
    fprintf(outputfile, "\t%s\t$2E\t\t\t; CS\n", DB);
    emit_data(0, STRING_TYPE, conststr, 0);
}
void emit_addi(int cval)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$38,$%02X\t\t\t; ADDI\t%d\n", DB, cval, cval);
}
void emit_subi(int cval)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$3A,$%02X\t\t\t; SUBI\t%d\n", DB, cval, cval);
}
void emit_andi(int cval)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$3C,$%02X\t\t\t; ANDI\t%d\n", DB, cval, cval);
}
void emit_ori(int cval)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$3E,$%02X\t\t\t; ORI\t%d\n", DB, cval, cval);
}
void emit_lb(void)
{
    fprintf(outputfile, "\t%s\t$60\t\t\t; LB\n", DB);
}
void emit_lw(void)
{
    fprintf(outputfile, "\t%s\t$62\t\t\t; LW\n", DB);
}
void emit_llb(int index)
{
    fprintf(outputfile, "\t%s\t$64,$%02X\t\t\t; LLB\t[%d]\n", DB, index, index);
}
void emit_llw(int index)
{
    fprintf(outputfile, "\t%s\t$66,$%02X\t\t\t; LLW\t[%d]\n", DB, index, index);
}
void emit_addlb(int index)
{
    fprintf(outputfile, "\t%s\t$B0,$%02X\t\t\t; ADDLB\t[%d]\n", DB, index, index);
}
void emit_addlw(int index)
{
    fprintf(outputfile, "\t%s\t$B2,$%02X\t\t\t; ADDLW\t[%d]\n", DB, index, index);
}
void emit_idxlb(int index)
{
    fprintf(outputfile, "\t%s\t$B8,$%02X\t\t\t; IDXLB\t[%d]\n", DB, index, index);
}
void emit_idxlw(int index)
{
    fprintf(outputfile, "\t%s\t$BA,$%02X\t\t\t; IDXLW\t[%d]\n", DB, index, index);
}
void emit_lab(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$68\t\t\t; LAB\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$68,$%02X,$%02X\t\t; LAB\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_law(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$6A\t\t\t; LAW\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$6A,$%02X,$%02X\t\t; LAW\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_addab(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$B4\t\t\t; ADDAB\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$B4,$%02X,$%02X\t\t; ADDAB\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_addaw(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$B6\t\t\t; ADDAW\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$B6,$%02X,$%02X\t\t; ADDAW\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_idxab(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$BC\t\t\t; IDXAB\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$BC,$%02X,$%02X\t\t; IDXAB\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_idxaw(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$BE\t\t\t; IDXAW\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$BE,$%02X,$%02X\t\t; IDXAW\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_sb(void)
{
    fprintf(outputfile, "\t%s\t$70\t\t\t; SB\n", DB);
}
void emit_sw(void)
{
    fprintf(outputfile, "\t%s\t$72\t\t\t; SW\n", DB);
}
void emit_slb(int index)
{
    fprintf(outputfile, "\t%s\t$74,$%02X\t\t\t; SLB\t[%d]\n", DB, index, index);
}
void emit_slw(int index)
{
    fprintf(outputfile, "\t%s\t$76,$%02X\t\t\t; SLW\t[%d]\n", DB, index, index);
}
void emit_dlb(int index)
{
    fprintf(outputfile, "\t%s\t$6C,$%02X\t\t\t; DLB\t[%d]\n", DB, index, index);
}
void emit_dlw(int index)
{
    fprintf(outputfile, "\t%s\t$6E,$%02X\t\t\t; DLW\t[%d]\n", DB, index, index);
}
void emit_sab(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$78\t\t\t; SAB\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$78,$%02X,$%02X\t\t; SAB\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_saw(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$7A\t\t\t; SAW\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
    {
        fprintf(outputfile, "\t%s\t$7A,$%02X,$%02X\t\t; SAW\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
    }
}
void emit_dab(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$7C\t\t\t; DAB\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
        fprintf(outputfile, "\t%s\t$7C,$%02X,$%02X\t\t; DAB\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
}
void emit_daw(int tag, int offset, int type)
{
    if (type)
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$7E\t\t\t; DAW\t%s+%d\n", DB, taglbl, offset);
        fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
    }
    else
        fprintf(outputfile, "\t%s\t$7E,$%02X,$%02X\t\t; DAW\t%d\n", DB, offset&0xFF,(offset>>8)&0xFF, offset);
}
void emit_localaddr(int index)
{
    fprintf(outputfile, "\t%s\t$28,$%02X\t\t\t; LLA\t[%d]\n", DB, index, index);
}
void emit_globaladdr(int tag, int offset, int type)
{
    int fixup = fixup_new(tag, type, FIXUP_WORD);
    char *taglbl = tag_string(tag, type);
    fprintf(outputfile, "\t%s\t$26\t\t\t; LA\t%s+%d\n", DB, taglbl, offset);
    fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl, offset);
}
void emit_indexbyte(void)
{
    fprintf(outputfile, "\t%s\t$82\t\t\t; IDXB\n", DB);
}
void emit_indexword(void)
{
    fprintf(outputfile, "\t%s\t$9E\t\t\t; IDXW\n", DB);
}
void emit_select(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$52\t\t\t; SEL\n", DB);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_caseblock(int casecnt, int *caseof, int *casetyp, int *casetag)
{
    int i;

    if (casecnt < 1 || casecnt > 256)
        parse_error("Switch count under/overflow\n");
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$%02X\t\t\t; CASEBLOCK\n", DB, casecnt & 0xFF);
    for (i = 0; i < casecnt; i++)
    {
        if (casetyp[i] & (FUNC_TYPE | ADDR_TYPE))
        {
            int fixup = fixup_new(caseof[i], casetyp[i], FIXUP_WORD);
            char *taglbl = tag_string(caseof[i], casetyp[i]);
            fprintf(outputfile, "_F%03d%c\t%s\t%s+%d\t\t\n", fixup, LBL, DW, casetyp[i] & EXTERN_TYPE ? "0" : taglbl, 0);
        }
        else
            fprintf(outputfile, "\t%s\t$%04X\n", DW, caseof[i] & 0xFFFF);
        fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, casetag[i]);
    }
}
void emit_breq(int tag)
{
    fprintf(outputfile, "\t%s\t$22\t\t\t; BREQ\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_brne(int tag)
{
    fprintf(outputfile, "\t%s\t$24\t\t\t; BRNE\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_brfls(int tag)
{
    fprintf(outputfile, "\t%s\t$4C\t\t\t; BRFLS\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_brtru(int tag)
{
    fprintf(outputfile, "\t%s\t$4E\t\t\t; BRTRU\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_brnch(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$50\t\t\t; BRNCH\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_brand(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$AC\t\t\t; BRAND\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_bror(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$AE\t\t\t; BROR\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_brgt(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$A0\t\t\t; BRGT\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_brlt(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$A2\t\t\t; BRLT\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_incbrle(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$A4\t\t\t; INCBRLE\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_addbrle(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$A6\t\t\t; ADDBRLE\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_decbrge(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$A8\t\t\t; DECBRGE\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_subbrge(int tag)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$AA\t\t\t; SUBBRGE\t_B%03d\n", DB, tag);
    fprintf(outputfile, "\t%s\t_B%03d-*\n", DW, tag);
}
void emit_call(int tag, int type)
{
    if (type == CONST_TYPE)
    {
        fprintf(outputfile, "\t%s\t$54\t\t\t; CALL\t%i\n", DB, tag);
        fprintf(outputfile, "\t%s\t%i\t\t\n", DW, tag);
    }
    else
    {
        int fixup = fixup_new(tag, type, FIXUP_WORD);
        char *taglbl = tag_string(tag, type);
        fprintf(outputfile, "\t%s\t$54\t\t\t; CALL\t%s\n", DB, taglbl);
        fprintf(outputfile, "_F%03d%c\t%s\t%s\t\t\n", fixup, LBL, DW, type & EXTERN_TYPE ? "0" : taglbl);
    }
}
void emit_ical(void)
{
    fprintf(outputfile, "\t%s\t$56\t\t\t; ICAL\n", DB);
}
void emit_leave(void)
{
    emit_pending_seq();
    if (localsize)
        fprintf(outputfile, "\t%s\t$5A,$%02X\t\t\t; LEAVE\t%d\n", DB, localsize, localsize);
    else
        fprintf(outputfile, "\t%s\t$5C\t\t\t; RET\n", DB);
}
void emit_ret(void)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$5C\t\t\t; RET\n", DB);
}
void emit_enter(int cparams)
{
    if (localsize)
        fprintf(outputfile, "\t%s\t$58,$%02X,$%02X\t\t; ENTER\t%d,%d\n", DB, localsize, cparams, localsize, cparams);
}
void emit_start(void)
{
    fprintf(outputfile, "_INIT%c\n", LBL);
    outflags |= INIT;
    defs++;
}
void emit_drop(void)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$30\t\t\t; DROP \n", DB);
}
void emit_drop2(void)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$32\t\t\t; DROP2\n", DB);
}
void emit_dup(void)
{
    emit_pending_seq();
    fprintf(outputfile, "\t%s\t$34\t\t\t; DUP\n", DB);
}
int emit_unaryop(t_token op)
{
    emit_pending_seq();
    switch (op)
    {
        case NEG_TOKEN:
            fprintf(outputfile, "\t%s\t$90\t\t\t; NEG\n", DB);
            break;
        case COMP_TOKEN:
            fprintf(outputfile, "\t%s\t$92\t\t\t; COMP\n", DB);
            break;
        case LOGIC_NOT_TOKEN:
            fprintf(outputfile, "\t%s\t$80\t\t\t; NOT\n", DB);
            break;
        case INC_TOKEN:
            fprintf(outputfile, "\t%s\t$8C\t\t\t; INCR\n", DB);
            break;
        case DEC_TOKEN:
            fprintf(outputfile, "\t%s\t$8E\t\t\t; DECR\n", DB);
            break;
        case BPTR_TOKEN:
            emit_lb();
            break;
        case WPTR_TOKEN:
            emit_lw();
            break;
        default:
            printf("emit_unaryop(%c) ???\n", op  & 0x7F);
            return (0);
    }
    return (1);
}
int emit_op(t_token op)
{
    emit_pending_seq();
    switch (op)
    {
        case MUL_TOKEN:
            fprintf(outputfile, "\t%s\t$86\t\t\t; MUL\n", DB);
            break;
        case DIV_TOKEN:
            fprintf(outputfile, "\t%s\t$88\t\t\t; DIV\n", DB);
            break;
        case MOD_TOKEN:
            fprintf(outputfile, "\t%s\t$8A\t\t\t; MOD\n", DB);
            break;
        case ADD_TOKEN:
            fprintf(outputfile, "\t%s\t$82\t\t\t; ADD \n", DB);
            break;
        case SUB_TOKEN:
            fprintf(outputfile, "\t%s\t$84\t\t\t; SUB \n", DB);
            break;
        case SHL_TOKEN:
            fprintf(outputfile, "\t%s\t$9A\t\t\t; SHL\n", DB);
            break;
        case SHR_TOKEN:
            fprintf(outputfile, "\t%s\t$9C\t\t\t; SHR\n", DB);
            break;
        case AND_TOKEN:
            fprintf(outputfile, "\t%s\t$94\t\t\t; AND \n", DB);
            break;
        case OR_TOKEN:
            fprintf(outputfile, "\t%s\t$96\t\t\t; OR \n", DB);
            break;
        case EOR_TOKEN:
            fprintf(outputfile, "\t%s\t$98\t\t\t; XOR\n", DB);
            break;
        case EQ_TOKEN:
            fprintf(outputfile, "\t%s\t$40\t\t\t; ISEQ\n", DB);
            break;
        case NE_TOKEN:
            fprintf(outputfile, "\t%s\t$42\t\t\t; ISNE\n", DB);
            break;
        case GE_TOKEN:
            fprintf(outputfile, "\t%s\t$48\t\t\t; ISGE\n", DB);
            break;
        case LT_TOKEN:
            fprintf(outputfile, "\t%s\t$46\t\t\t; ISLT\n", DB);
            break;
        case GT_TOKEN:
            fprintf(outputfile, "\t%s\t$44\t\t\t; ISGT\n", DB);
            break;
        case LE_TOKEN:
            fprintf(outputfile, "\t%s\t$4A\t\t\t; ISLE\n", DB);
            break;
        case COMMA_TOKEN:
            break;
        default:
            return (0);
    }
    return (1);
}
/*
 * New/release sequence ops
 */
t_opseq *new_op(void)
{
    t_opseq* op = freeop_lst;
    if (!op)
    {
        fprintf(stderr, "Compiler out of sequence ops!\n");
        return (NULL);
    }
    freeop_lst = freeop_lst->nextop;
    op->nextop = NULL;
    return (op);
}
void release_op(t_opseq *op)
{
    if (op)
    {
        op->nextop = freeop_lst;
        freeop_lst = op;
    }
}
void release_seq(t_opseq *seq)
{
    t_opseq *op;
    while (seq)
    {
        op = seq;
        seq = seq->nextop;
        /*
         * Free this op
         */
        op->nextop = freeop_lst;
        freeop_lst = op;
    }
}
/*
 * Indicate if an address is (or might be) memory-mapped hardware; used to avoid
 * optimising away accesses to such addresses.
 */
int is_hardware_address(int addr)
{
    // TODO: I think this is reasonable for Apple hardware but I'm not sure.
    // It's a bit too strong for Acorn hardware but code is unlikely to try to
    // read from high addresses anyway, so there's no real harm in not
    // optimising such accesses anyway.
    return addr >= 0xC000;
}
/*
 * Replace all but the first of a series of identical load opcodes by DUP. This
 * doesn't reduce the number of opcodes but does reduce their size in bytes.
 * This is only called on the second optimisation pass because the DUP opcodes
 * may inhibit other peephole optimisations which are more valuable.
 */
int try_dupify(t_opseq *op)
{
    int crunched = 0;
    t_opseq *opn = op->nextop;
    for (; opn; opn = opn->nextop)
    {
        if (op->code != opn->code)
            return crunched;
        switch (op->code)
        {
            case CONST_CODE:
                if (op->val != opn->val)
                    return crunched;
                break;
            case LADDR_CODE:
            case LLB_CODE:
            case LLW_CODE:
                if (op->offsz != opn->offsz)
                    return crunched;
                break;
            case GADDR_CODE:
            case LAB_CODE:
            case LAW_CODE:
                if ((op->tag != opn->tag) || (op->offsz != opn->offsz) /*|| (op->type != opn->type)*/)
                    return crunched;
                break;

            default:
                return crunched;
        }
        opn->code = DUP_CODE;
        crunched  = 1;
    }
    return crunched;
}
/*
 * Crunch sequence (peephole optimize)
 */
int crunch_seq(t_opseq **seq, int pass)
{
    t_opseq *opnext, *opnextnext, *opprev = 0;
    t_opseq *op = *seq;
    int crunched = 0;
    int freeops  = 0;
    int shiftcnt;

    while (op && (opnext = op->nextop))
    {
        switch (op->code)
        {
            case CONST_CODE:
                if (op->val == 1)
                {
                    if (opnext->code == BINARY_CODE(ADD_TOKEN))
                    {
                        op->code = INC_CODE;
                        freeops = 1;
                        break;
                    }
                    if (opnext->code == BINARY_CODE(SUB_TOKEN))
                    {
                        op->code = DEC_CODE;
                        freeops = 1;
                        break;
                    }
                    if (opnext->code == BINARY_CODE(SHL_TOKEN))
                    {
                        op->code = DUP_CODE;
                        opnext->code = BINARY_CODE(ADD_TOKEN);
                        break;
                    }
                    if (opnext->code == BINARY_CODE(MUL_TOKEN) || opnext->code == BINARY_CODE(DIV_TOKEN))
                    {
                        freeops = -2;
                        break;
                    }
                }
                switch (opnext->code)
                {
                    case NEG_CODE:
                        op->val = -(op->val);
                        freeops = 1;
                        break;
                    case COMP_CODE:
                        op->val = ~(op->val);
                        freeops = 1;
                        break;
                    case LOGIC_NOT_CODE:
                        op->val = op->val ? 0 : 1;
                        freeops = 1;
                        break;
                    case UNARY_CODE(BPTR_TOKEN):
                    case LB_CODE:
                        op->offsz = op->val;
                        op->code  = LAB_CODE;
                        freeops   = 1;
                        break;
                    case UNARY_CODE(WPTR_TOKEN):
                    case LW_CODE:
                        op->offsz = op->val;
                        op->code  = LAW_CODE;
                        freeops   = 1;
                        break;
                    case SB_CODE:
                        op->offsz = op->val;
                        op->code  = SAB_CODE;
                        freeops   = 1;
                        break;
                    case SW_CODE:
                        op->offsz = op->val;
                        op->code  = SAW_CODE;
                        freeops   = 1;
                        break;
                    case BRFALSE_CODE:
                        if (op->val)
                            freeops = -2; // Remove constant and never taken branch
                        else
                        {
                            op->code = BRNCH_CODE; // Always taken branch
                            op->tag  = opnext->tag;
                            freeops  = 1;
                        }
                        break;
                    case BRTRUE_CODE:
                        if (!op->val)
                            freeops = -2; // Remove constant never taken branch
                        else
                        {
                            op->code = BRNCH_CODE; // Always taken branch
                            op->tag  = opnext->tag;
                            freeops  = 1;
                        }
                        break;
                    case BRGT_CODE:
                        if (opprev && (opprev->code == CONST_CODE) && (op->val <= opprev->val))
                            freeops = 1;
                        break;
                    case BRLT_CODE:
                        if (opprev && (opprev->code == CONST_CODE) && (op->val >= opprev->val))
                            freeops = 1;
                        break;
                    case BROR_CODE:
                        if (!op->val)
                            freeops = -2; // Remove zero constant
                        break;
                    case BRAND_CODE:
                        if (op->val)
                            freeops = -2; // Remove non-zero constant
                        break;
                    case NE_CODE:
                        if (!op->val)
                            freeops = -2; // Remove ZERO:ISNE
                        break;
                    case EQ_CODE:
                        if (!op->val)
                        {
                            op->code = LOGIC_NOT_CODE;
                            freeops = 1;
                        }
                        break;
                    case CONST_CODE:
                        // Collapse constant operation
                        if ((opnextnext = opnext->nextop))
                            switch (opnextnext->code)
                            {
                                case BINARY_CODE(MUL_TOKEN):
                                    op->val *= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(DIV_TOKEN):
                                    op->val /= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(MOD_TOKEN):
                                    op->val %= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(ADD_TOKEN):
                                    op->val += opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(SUB_TOKEN):
                                    op->val -= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(SHL_TOKEN):
                                    op->val <<= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(SHR_TOKEN):
                                    op->val >>= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(AND_TOKEN):
                                    op->val &= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(OR_TOKEN):
                                    op->val |= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(EOR_TOKEN):
                                    op->val ^= opnext->val;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(EQ_TOKEN):
                                    op->val = op->val == opnext->val ? 1 : 0;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(NE_TOKEN):
                                    op->val = op->val != opnext->val ? 1 : 0;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(GE_TOKEN):
                                    op->val = op->val >= opnext->val ? 1 : 0;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(LT_TOKEN):
                                    op->val = op->val < opnext->val ? 1 : 0;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(GT_TOKEN):
                                    op->val = op->val > opnext->val ? 1 : 0;
                                    freeops  = 2;
                                    break;
                                case BINARY_CODE(LE_TOKEN):
                                    op->val = op->val <= opnext->val ? 1 : 0;
                                    freeops  = 2;
                                    break;
                            }
                        // End of collapse constant operation
                        if ((pass > 0) && (freeops == 0) && (op->val != 0))
                            crunched = try_dupify(op);
                        break; // CONST_CODE
                    case BINARY_CODE(ADD_TOKEN):
                        if (op->val == 0)
                        {
                            freeops = -2;
                        }
                        else if (op->val > 0 && op->val <= 255)
                        {
                            op->code = ADDI_CODE;
                            freeops  = 1;
                        }
                        else if (op->val >= -255 && op->val < 0)
                        {
                            op->code = SUBI_CODE;
                            op->val  = -op->val;
                            freeops  = 1;
                        }
                        break;
                    case BINARY_CODE(SUB_TOKEN):
                        if (op->val == 0)
                        {
                            freeops = -2;
                        }
                        else if (op->val > 0 && op->val <= 255)
                        {
                            op->code = SUBI_CODE;
                            freeops  = 1;
                        }
                        else if (op->val >= -255 && op->val < 0)
                        {
                            op->code = ADDI_CODE;
                            op->val  = -op->val;
                            freeops  = 1;
                        }
                        break;
                    case BINARY_CODE(AND_TOKEN):
                        if (op->val >= 0 && op->val <= 255)
                        {
                            op->code = ANDI_CODE;
                            freeops  = 1;
                        }
                        break;
                    case BINARY_CODE(OR_TOKEN):
                        if (op->val == 0)
                        {
                            freeops = -2;
                        }
                        else if (op->val > 0 && op->val <= 255)
                        {
                            op->code = ORI_CODE;
                            freeops  = 1;
                        }
                        break;
                    case BINARY_CODE(MUL_TOKEN):
                        if (op->val == 0)
                        {
                            op->code = DROP_CODE;
                            opnext->code = CONST_CODE;
                            opnext->val = 0;
                        }
                        else if (op->val == 2)
                        {
                            op->code = DUP_CODE;
                            opnext->code = BINARY_CODE(ADD_TOKEN);
                        }
                        else
                        {
                            //
                            // Constants 0, 1 and 2 handled above
                            //
                            for (shiftcnt = 2; shiftcnt < 16; shiftcnt++)
                            {
                                if (op->val == (1 << shiftcnt))
                                {
                                    op->val = shiftcnt;
                                    opnext->code = BINARY_CODE(SHL_TOKEN);
                                    break;
                                }
                            }
                        }
                        break;
                    case BINARY_CODE(DIV_TOKEN):
                        //
                        // Constant 1 handled above
                        //
                        for (shiftcnt = 1; shiftcnt < 16; shiftcnt++)
                        {
                            if (op->val == (1 << shiftcnt))
                            {
                                op->val = shiftcnt;
                                opnext->code = BINARY_CODE(SHR_TOKEN);
                                break;
                            }
                        }
                        break;
                }
                break; // CONST_CODE
            case LADDR_CODE:
                switch (opnext->code)
                {
                    case CONST_CODE:
                        if ((opnextnext = opnext->nextop))
                            switch (opnextnext->code)
                            {
                                case ADD_CODE:
                                case INDEXB_CODE:
                                    op->offsz += opnext->val;
                                    freeops = 2;
                                    break;
                                case INDEXW_CODE:
                                    op->offsz += opnext->val * 2;
                                    freeops = 2;
                                    break;
                            }
                        break;
                    case LB_CODE:
                        op->code  = LLB_CODE;
                        freeops   = 1;
                        break;
                    case LW_CODE:
                        op->code  = LLW_CODE;
                        freeops   = 1;
                        break;
                    case SB_CODE:
                        op->code  = SLB_CODE;
                        freeops   = 1;
                        break;
                    case SW_CODE:
                        op->code  = SLW_CODE;
                        freeops   = 1;
                        break;
                }
                if ((pass > 0) && (freeops == 0))
                    crunched = try_dupify(op);
                break; // LADDR_CODE
            case GADDR_CODE:
                switch (opnext->code)
                {
                    case CONST_CODE:
                        if ((opnextnext = opnext->nextop))
                            switch (opnextnext->code)
                            {
                                case ADD_CODE:
                                case INDEXB_CODE:
                                    op->offsz += opnext->val;
                                    freeops = 2;
                                    break;
                                case INDEXW_CODE:
                                    op->offsz += opnext->val * 2;
                                    freeops = 2;
                                    break;
                            }
                        break;
                    case LB_CODE:
                        op->code  = LAB_CODE;
                        freeops   = 1;
                        break;
                    case LW_CODE:
                        op->code  = LAW_CODE;
                        freeops   = 1;
                        break;
                    case SB_CODE:
                        op->code  = SAB_CODE;
                        freeops   = 1;
                        break;
                    case SW_CODE:
                        op->code  = SAW_CODE;
                        freeops   = 1;
                        break;
                    case ICAL_CODE:
                        op->code  = CALL_CODE;
                        freeops   = 1;
                        break;
                }
                if ((pass > 0) && (freeops == 0))
                    crunched = try_dupify(op);
                break; // GADDR_CODE
            case LLB_CODE:
                if ((opnext->code == ADD_CODE) || (opnext->code == INDEXB_CODE))
                {
                    op->code = ADDLB_CODE;
                    freeops  = 1;
                }
                else if (opnext->code == INDEXW_CODE)
                {
                    op->code = IDXLB_CODE;
                    freeops  = 1;
                }
                else if (pass > 0)
                    crunched = try_dupify(op);
                break; // LLB_CODE
            case LLW_CODE:
                // LLW [n]:CB 8:SHR -> LLB [n+1]
                if ((opnext->code == CONST_CODE) && (opnext->val == 8))
                {
                    if ((opnextnext = opnext->nextop))
                    {
                        if (opnextnext->code == SHR_CODE)
                        {
                            op->code = LLB_CODE;
                            op->offsz++;
                            freeops = 2;
                            break;
                        }
                    }
                }
                else if ((opnext->code == ADD_CODE) || (opnext->code == INDEXB_CODE))
                {
                    op->code = ADDLW_CODE;
                    freeops  = 1;
                }
                else if (opnext->code == INDEXW_CODE)
                {
                    op->code = IDXLW_CODE;
                    freeops  = 1;
                }
                else if (pass > 0)
                    crunched = try_dupify(op);
                break; // LLW_CODE
            case LAB_CODE:
                if ((opnext->code == ADD_CODE) || (opnext->code == INDEXB_CODE))
                {
                    op->code = ADDAB_CODE;
                    freeops  = 1;
                }
                else if (opnext->code == INDEXW_CODE)
                {
                    op->code = IDXAB_CODE;
                    freeops  = 1;
                }
                else if ((pass > 0) && (op->type || !is_hardware_address(op->offsz)))
                    crunched = try_dupify(op);
                break; // LAB_CODE
            case LAW_CODE:
                // LAW x:CB 8:SHR -> LAB x+1
                if ((opnext->code == CONST_CODE) && (opnext->val == 8))
                {
                    if ((opnextnext = opnext->nextop))
                    {
                        if (opnextnext->code == SHR_CODE)
                        {
                            op->code = LAB_CODE;
                            op->offsz++;
                            freeops = 2;
                            break;
                        }
                    }
                }
                else if ((opnext->code == ADD_CODE) || (opnext->code == INDEXB_CODE))
                {
                    op->code = ADDAW_CODE;
                    freeops  = 1;
                }
                else if (opnext->code == INDEXW_CODE)
                {
                    op->code = IDXAW_CODE;
                    freeops  = 1;
                }
                else if ((pass > 0) && (op->type || !is_hardware_address(op->offsz)))
                    crunched = try_dupify(op);
                break; // LAW_CODE
            case LOGIC_NOT_CODE:
                switch (opnext->code)
                {
                    case BRFALSE_CODE:
                        op->code = BRTRUE_CODE;
                        op->tag  = opnext->tag;
                        freeops  = 1;
                        break;
                    case BRTRUE_CODE:
                        op->code = BRFALSE_CODE;
                        op->tag  = opnext->tag;
                        freeops  = 1;
                        break;
                }
                break; // LOGIC_NOT_CODE
            case EQ_CODE:
                switch (opnext->code)
                {
                    case BRFALSE_CODE:
                        op->code = BRNE_CODE;
                        op->tag  = opnext->tag;
                        freeops  = 1;
                        break;
                    case BRTRUE_CODE:
                        op->code = BREQ_CODE;
                        op->tag  = opnext->tag;
                        freeops  = 1;
                        break;
                }
                break; // EQ_CODE
            case NE_CODE:
                switch (opnext->code)
                {
                    case BRFALSE_CODE:
                        op->code = BREQ_CODE;
                        op->tag  = opnext->tag;
                        freeops  = 1;
                        break;
                    case BRTRUE_CODE:
                        op->code = BRNE_CODE;
                        op->tag  = opnext->tag;
                        freeops  = 1;
                        break;
                }
                break; // NE_CODE
            case SLB_CODE:
                if ((opnext->code == LLB_CODE) && (op->offsz == opnext->offsz))
                {
                    op->code = DLB_CODE;
                    freeops = 1;
                }
                break; // SLB_CODE
            case SLW_CODE:
                if ((opnext->code == LLW_CODE) && (op->offsz == opnext->offsz))
                {
                    op->code = DLW_CODE;
                    freeops = 1;
                }
                break; // SLW_CODE
            case SAB_CODE:
                if ((opnext->code == LAB_CODE) && (op->tag == opnext->tag) &&
                    (op->offsz == opnext->offsz) && (op->type == opnext->type))
                {
                    op->code = DAB_CODE;
                    freeops = 1;
                }
                break; // SAB_CODE
            case SAW_CODE:
                if ((opnext->code == LAW_CODE) && (op->tag == opnext->tag) &&
                    (op->offsz == opnext->offsz) && (op->type == opnext->type))
                {
                    op->code = DAW_CODE;
                    freeops = 1;
                }
                break; // SAW_CODE
        }
        //
        // Free up crunched ops. If freeops is positive we free up that many ops
        // *after* op; if it's negative, we free up abs(freeops) ops *starting
        // with* op.
        if (freeops < 0)
        {
            freeops = -freeops;
            // If op is at the start of the sequence, we treat this as a special
            // case.
            if (op == *seq)
            {
                for (; freeops > 0; --freeops)
                {
                    opnext = op->nextop;
                    release_op(op);
                    op = *seq = opnext;
                }
                crunched = 1;
            }
            // Otherwise we just move op back to point to the previous op and
            // let the following loop remove the required number of ops.
            else
            {
                op      = opprev;
                opnext  = op->nextop;
            }
        }
        while (freeops)
        {
            op->nextop     = opnext->nextop;
            opnext->nextop = freeop_lst;
            freeop_lst     = opnext;
            opnext         = op->nextop;
            crunched       = 1;
            freeops--;
        }
        opprev = op;
        op = opnext;
    }
    return (crunched);
}
/*
 * Generate a sequence of code
 */
t_opseq *gen_seq(t_opseq *seq, int opcode, long cval, int tag, int offsz, int type)
{
    t_opseq *op;

    if (!seq)
    {
        op = seq = new_op();
    }
    else
    {
        op = seq;
        while (op->nextop)
            op = op->nextop;
        op->nextop = new_op();
        op = op->nextop;
    }
    op->code  = opcode;
    op->val   = cval;
    op->tag   = tag;
    op->offsz = offsz;
    op->type  = type;
    return (seq);
}
/*
 * Append one sequence to the end of another
 */
t_opseq *cat_seq(t_opseq *seq1, t_opseq *seq2)
{
    t_opseq *op;

    if (!seq1)
        return (seq2);
    for (op = seq1; op->nextop; op = op->nextop);
    op->nextop = seq2;
    return (seq1);
}
/*
 * Emit a sequence of ops (into the pending sequence)
 */
int emit_seq(t_opseq *seq)
{
    t_opseq *op;
    int emitted = 0;
    int string = 0;
    for (op = seq; op; op = op->nextop)
    {
        if (op->code == STR_CODE)
            string = 1;
        emitted++;
    }
    pending_seq = cat_seq(pending_seq, seq);
    // The source code comments in the output are much more logical if we don't
    // merge multiple sequences together. There's no value in doing this merging
    // if we're not optimizing, and we optionally allow it to be prevented even
    // when we are optimizing by specifing the -N (NO_COMBINE) flag.
    //
    // We must also force output if the sequence includes a CS opcode, as the
    // associated 'constant' is only temporarily valid.
    if (!(outflags & OPTIMIZE) || (outflags & NO_COMBINE) || string)
        return emit_pending_seq();
    return (emitted);
}
/*
 * Emit the pending sequence
 */
int emit_pending_seq()
{
    // This is called by some of the emit_*() functions to ensure that any
    // pending ops are emitted before they emit their own op when they are
    // called from the parser. However, this function itself calls some of those
    // emit_*() functions to emit instructions from the pending sequence, which
    // would cause an infinite loop if we weren't careful. We therefore set
    // pending_seq to null on entry and work with a local copy, so if this
    // function calls back into itself it is a no-op.
    if (!pending_seq)
        return 0;
    t_opseq *local_pending_seq = pending_seq;
    pending_seq = 0;

    t_opseq *op;
    int emitted = 0;

    if (outflags & OPTIMIZE)
    {
        int pass;
        for (pass = 0; pass < 2; pass++)
            while (crunch_seq(&local_pending_seq, pass));
    }
    while (local_pending_seq)
    {
        op = local_pending_seq;
        switch (op->code)
        {
            case NEG_CODE:
            case COMP_CODE:
            case LOGIC_NOT_CODE:
            case INC_CODE:
            case DEC_CODE:
            case BPTR_CODE:
            case WPTR_CODE:
                emit_unaryop(op->code);
                break;
            case MUL_CODE:
            case DIV_CODE:
            case MOD_CODE:
            case ADD_CODE:
            case SUB_CODE:
            case SHL_CODE:
            case SHR_CODE:
            case AND_CODE:
            case OR_CODE:
            case EOR_CODE:
            case EQ_CODE:
            case NE_CODE:
            case GE_CODE:
            case LT_CODE:
            case GT_CODE:
            case LE_CODE:
                emit_op(op->code);
                break;
            case CONST_CODE:
                emit_const(op->val);
                break;
            case STR_CODE:
                emit_conststr(op->val);
                break;
            case ADDI_CODE:
                emit_addi(op->val);
                break;
            case SUBI_CODE:
                emit_subi(op->val);
                break;
            case ANDI_CODE:
                emit_andi(op->val);
                break;
            case ORI_CODE:
                emit_ori(op->val);
                break;
            case LB_CODE:
                emit_lb();
                break;
            case LW_CODE:
                emit_lw();
                break;
            case LLB_CODE:
                emit_llb(op->offsz);
                break;
            case LLW_CODE:
                emit_llw(op->offsz);
                break;
            case ADDLB_CODE:
                emit_addlb(op->offsz);
                break;
            case ADDLW_CODE:
                emit_addlw(op->offsz);
                break;
            case IDXLB_CODE:
                emit_idxlb(op->offsz);
                break;
            case IDXLW_CODE:
                emit_idxlw(op->offsz);
                break;
            case LAB_CODE:
                emit_lab(op->tag, op->offsz, op->type);
                break;
            case LAW_CODE:
                emit_law(op->tag, op->offsz, op->type);
                break;
            case ADDAB_CODE:
                emit_addab(op->tag, op->offsz, op->type);
                break;
            case ADDAW_CODE:
                emit_addaw(op->tag, op->offsz, op->type);
                break;
            case IDXAB_CODE:
                emit_idxab(op->tag, op->offsz, op->type);
                break;
            case IDXAW_CODE:
                emit_idxaw(op->tag, op->offsz, op->type);
                break;
            case SB_CODE:
                emit_sb();
                break;
            case SW_CODE:
                emit_sw();
                break;
            case SLB_CODE:
                emit_slb(op->offsz);
                break;
            case SLW_CODE:
                emit_slw(op->offsz);
                break;
            case DLB_CODE:
                emit_dlb(op->offsz);
                break;
            case DLW_CODE:
                emit_dlw(op->offsz);
                break;
            case SAB_CODE:
                emit_sab(op->tag, op->offsz, op->type);
                break;
            case SAW_CODE:
                emit_saw(op->tag, op->offsz, op->type);
                break;
            case DAB_CODE:
                emit_dab(op->tag, op->offsz, op->type);
                break;
            case DAW_CODE:
                emit_daw(op->tag, op->offsz, op->type);
                break;
            case CALL_CODE:
                emit_call(op->tag, op->type);
                break;
            case ICAL_CODE:
                emit_ical();
                break;
            case LADDR_CODE:
                emit_localaddr(op->offsz);
                break;
            case GADDR_CODE:
                emit_globaladdr(op->tag, op->offsz, op->type);
                break;
            case INDEXB_CODE:
                emit_indexbyte();
                break;
            case INDEXW_CODE:
                emit_indexword();
                break;
            case DROP_CODE:
                emit_drop();
                break;
            case DUP_CODE:
                emit_dup();
                break;
            case BRNCH_CODE:
                emit_brnch(op->tag);
                break;
            case BRAND_CODE:
                emit_brand(op->tag);
                break;
            case BROR_CODE:
                emit_bror(op->tag);
                break;
            case BREQ_CODE:
                emit_breq(op->tag);
                break;
            case BRNE_CODE:
                emit_brne(op->tag);
                break;
            case BRFALSE_CODE:
                emit_brfls(op->tag);
                break;
            case BRTRUE_CODE:
                emit_brtru(op->tag);
                break;
            case BRGT_CODE:
                emit_brgt(op->tag);
                break;
            case BRLT_CODE:
                emit_brlt(op->tag);
                break;
            case CODETAG_CODE:
                fprintf(outputfile, "_B%03d%c\n", op->tag, LBL);
                break;
            case NOP_CODE:
                break;
            default:
                return (0);
        }
        emitted++;
        local_pending_seq = local_pending_seq->nextop;
        /*
         * Free this op
         */
        op->nextop = freeop_lst;
        freeop_lst = op;
    }
    return (emitted);
}
