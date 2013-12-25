#include "stp_fs.h"
#include "stp.h"
#include "stp_error.h"

stp_error stp_errno = STP_NO_ERROR;

const char * const stp_errlist[STP_MAX_ERRNO + 1] = {
    N_("No error"), 				/*STP_NO_ERROR 0*/
    N_("Index file open error"), 	/*STP_INDEX_OPEN_ERROR 1*/
    N_("Metadata file open error"), /*STP_META_OPEN_ERROR 2*/
    N_("Index file read error"),    /*STP_INDEX_READ_ERROR 3*/
    N_("Metadata file read error"), /*STP_META_READ_ERROR 4*/
    N_("Index file write error"),   /*STP_INDEX_WRITE_ERROR 5*/
    N_("Metadata file write error"), /*STP_META_WRITE_ERROR 6*/
    N_("Bad magic number"),			 /*STP_BAD_MAGIC_NUMBER 7*/
    N_("Medadata file can't be write"),   /*STP_META_CANT_BE_WRITER 8*/
    N_("Index file can't be write"),	  /*STP_INDEX_CANT_BE_WRITER 9*/
    N_("Index file can't be reader"),     /*STP_INDEX_CANT_BE_READER 10*/
    N_("Metadata file can't be reader"),  /*STP_META_CANT_BE_READER 11*/
    N_("Index/Metadata reader can't store"),/*STP_INDEX_READER_CANT_STORE 12*/
    N_("Index/Metadata reader can't store"), /*STP_META_READER_CANT_STORE 13*/
    N_("Index/Metadata reader can't delete"),/*STP_INDEX_READER_CANT_DELETE 14*/
    N_("Index/Metadata reader can't delete"),/*STP_META_READER_CANT_DELETE 15*/
    N_("Index reader can't compact"),/*STP_INDEX_READER_CANT_COMPACTION 16*/
    N_("Meta reader can't compact"),/*STP_META_READER_CANT_COMPACTION 17*/
    N_("Index reader can't update"),/*STP_INDEX_READER_CANT_UPDATE 18*/
    N_("Meta reader can't update"),/*STP_META_READER_CANT_UPDATE 19*/
    N_("Index item not found"),/*STP_INDEX_ITEM_NOT_FOUND 20*/
    N_("Metadata item not found"),/*STP_META_ITEM_NOT_FOUND 21*/
    N_("Index file illeagal data"),/*STP_INDEX_ILLEAGAL_DATA 22*/
    N_("Meta file illeagal data"),/*STP_META_ILLEAGAL_DATA 23*/
    N_("Index file has no enough space"),/*STP_INDEX_NO_SPACE 24*/
    N_("Meta file has no enough space"),/*STP_META_NO_SPACE 25*/
    N_("Malloc memory error"),/*STP_MALLOC_SERROR 26*/
    N_("Create index file error"),/*STP_INDEX_CREAT_ERROR 27*/
    N_("Create metadata file error"),/*STP_META_CREAT_ERROR 28*/
    N_("Fail to check index file"),/*STP_INDEX_FILE_CHECK_ERROR 29*/
    N_("Fail to check metadata file"),/*STP_META_FILE_CHECK_ERROR 30*/
    N_("Fail to allocate inode"),/*STP_INODE_MALLOC_ERROR 31*/
    N_("Fail to allocate bnode"),/*STP_BNODE_MALLOC_ERROR 32*/
    N_("Invalid argument."),/*STP_INVALID_ARGUMENT 33*/
    N_("Function not implemented"),/*STP_NO_SYSCALL 34*/
    N_("Index key has existed"),/*STP_INDEX_EXIST 35*/
    N_("Index has reached maximum level"),/*STP_INDEX_MAX_LEVEL 36*/
    N_("Index key hasn't existed"),/*STP_INDEX_NOT_EXIST 37*/
    N_("File has existed"),/*STP_FS_ENTRY_EXIST 38*/
    N_("File hasn't existed"),/*STP_FS_ENTRY_NOEXIST 39*/
    N_("Directory entry is full"),/*STP_FS_ENTRY_FULL 40*/
    N_("Unknown error"),/*STP_UNKNOWN_ERROR 41*/
    N_("File ino han't existed"),/*STP_FS_INO_NOEXIST 42*/
    N_("File Directory isn't empty"),/*STP_FS_DIR_NOEMPTY 43*/
    N_("File isn't directory"),/*STP_FS_NO_DIR 44 */
    N_("Root can't delete"),/*STP_FS_ROOT 45 */
    N_("File entry isn't directory"),/*STP_FS_NODIR 46*/
    };

    
const char * stp_strerror(stp_error error)
{
    if((((int) error) < STP_MIN_ERRNO) || ((int)error > STP_MAX_ERRNO))
        return _("Unknown error");
    else
        return _(stp_errlist[(int)error]);
}
