#define ERROR_NONE          0
#define ERROR_ACCESS        -EACCES
#define ERROR_SEEK          -ESPIPE     
#define ERROR_ISDIR         -EISDIR
#define ERROR_NOSPACE       -ENOSPC
#define ERROR_EXISTS        -EEXIST
#define ERROR_NOTFOUND      -ENOENT
#define ERROR_UNSUPPORTED   -ENXIO
#define ERROR_IO            -EIO     /* Error Input/Output */
#define ERROR_INVAL         -EINVAL  /* Invalid Args */