// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(){
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst){
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum yfs_client::n2i(std::string n){
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string yfs_client::filename(inum inum){
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum){
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        return true;
    } 
    return false;
}

bool yfs_client::isdir(inum inum){
    extent_protocol::attr a;
    if(ec->getattr(inum, a) != extent_protocol::OK){
        printf("error getting attr\n");
        return false;
    }
    if(a.type == extent_protocol::T_DIR){
//        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin){
    int r = OK;

    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
//    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din){
    int r = OK;

//    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

/*
 * Only support set size of attr
 * get the content of inode ino, and modify its content
 * according to the size (< = >)content length
 */
int yfs_client::setattr(inum ino, size_t size){
    std::string content;
    if(ec->get(ino, content) != extent_protocol::OK)
        return IOERR;
    if(content.size() == size)
        return OK;
    content.resize(size);
    if(ec->put(ino, content) != extent_protocol::OK)
        return IOERR;
    return OK;
}

int yfs_client::add_entry(inum parent, const char *name, inum ino){
    std::string content;
    if(ec->get(parent, content) != extent_protocol::OK)
        return IOERR;
    content.append(name);
    content.push_back('\0');
    content.append(filename(ino));
    if(ec->put(parent, content) != extent_protocol::OK)
        return IOERR;
    return OK;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out){
    bool found = false;
    int r = lookup(parent, name, found, ino_out);
    if(found){
        return EXIST;
    }
    if(ec->create(extent_protocol::T_FILE, ino_out) != OK){
        printf("CREATE fail\n");
        return IOERR;
    }
    printf("CREATE file name:%s, ino:%0lld\n", name, ino_out);
    r = add_entry(parent, name, ino_out);
    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out){
    bool found = false;
    int r = lookup(parent, name, found, ino_out);
    if(found){
        printf("MKDIR found dir\n");
        return EXIST;
    }
    if(ec->create(extent_protocol::T_DIR, ino_out) != OK){
        printf("MKDIR fail\n");
        return IOERR;
    }
    r = add_entry(parent, name, ino_out);
    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out){
    std::list<dirent> list;
    if(readdir(parent, list) != OK){
        printf("LOOKUP error- inum: %016llx\n", parent);
        return IOERR;
    }
    for(std::list<dirent>::iterator it = list.begin(); it != list.end(); ++it){
        if(it->name == name){
            found = true;
            ino_out = it->inum;
            return OK;
        }
    }
    return OK;
}

int yfs_client::readdir(inum dir, std::list<dirent> &list){
    std::string content;
    if(ec->get(dir, content) != extent_protocol::OK){
        printf("READDIR fail\n");
        return IOERR;
    }
    std::istringstream ist(content);
    dirent entry;
    list.clear();
    while(getline(ist, entry.name, '\0')){
        ist >> entry.inum;
        list.push_back(entry);
    }
    return OK;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data){
    std::string content;
    if(size < 0 || off < 0 || ino <= 0){
        printf("READ argu error\n");
        return IOERR;
    }
    if(ec->get(ino, content) != OK){
        printf("READ get fail\n");
        return IOERR;
    }
    if(off >= (unsigned int)content.size()){
        printf("READ offset out of range\n");
        return IOERR;
    }
    content.erase(0, off);
    if(content.size() > size)
        content.resize(size);
    data = content;
    return OK;
}

/*
 * when off > length of original file, fill the holes with '\0'
 */
int yfs_client::write(inum ino, size_t size, off_t off, const char *data, size_t &bytes_written){
    std::string content;
    if(ec->get(ino, content) != extent_protocol::OK)
        return IOERR;
    bytes_written = off + size - content.size();
    if(off + size > content.size())
        content.resize(off + size, '\0');
    content.replace(off, size, data, size);
    if(ec->put(ino, content) != extent_protocol::OK)
        return IOERR;
    return OK;
}

int yfs_client::unlink(inum parent, const char *name){
    std::list<dirent> old_entries;
    if(readdir(parent, old_entries) != extent_protocol::OK)
        return IOERR;
    std::ostringstream ost;
    inum ino;
    bool found = false;
    for(std::list<dirent>::iterator it = old_entries.begin(); it != old_entries.end(); ++it){
        if(it->name != name){
            ost << it->name;
            ost.put('\0');
            ost << it->inum;
        }
        else{
            ino = it->inum;
            found = true;
        }
    }
    if(!found || ec->remove(ino) != extent_protocol::OK)
        return IOERR;
    if(ec->put(parent, ost.str()) != extent_protocol::OK)
        return IOERR;
    return OK;
}

int yfs_client::symlink(inum parent, const char *name, const char *path,  inum &ino_out){
    if(parent <= 0)
        return IOERR;
    printf("-----SYMLINK-----\nparent:%0lld name:%s path:%s",parent, name, path);
    if(ec->create(extent_protocol::T_SYMLINK, ino_out) != extent_protocol::OK)
        return IOERR;
    if(ec->put(ino_out, path) != extent_protocol::OK)
        return IOERR;
    if(add_entry(parent, name, ino_out) != extent_protocol::OK)
        return IOERR;
    printf(" ino:%0lld\n",ino_out);
    return OK;
}

int yfs_client::readlink(inum ino, std::string &path){
    if(ino <= 0)
        return IOERR;
    if(ec->get(ino, path) != extent_protocol::OK)
        return IOERR;
    printf("---------READLINK----------\nino:%0lld path:%s\n", ino, path.c_str());
    return OK;
}