# CSE-lab

## lab3

- 仔细梳理
- while循环等待releasing的线程
- acquire时添加线程到thread[lid]
- release时删去，以此判断是否还有线程在等锁
- new: revoke时若处于acquiring，唤醒沉睡线程

## lab4

### bugs of part2 & part3
* part2 put/cat error 118 : appendblock部分有误，传master_datanode
* part2卡（client拿不到锁，无法交互）：修改lock_client_cache初始化时的hostname解析
* part3 replicate失败：getlocation部分有错（加法器在循环中初始化
* 166 readOp fail与 170 wrong version：未解决

### part0 & part1
* 购买
* 在云主机页面可以查看所有主机的信息，手动将四个主机名改为app，name ，data1，data2
* 网页上建立SSH密钥，绑定四个主机，然后终端运行
```
ssh -i 密钥文件地址 ubuntu@公网ip
```

#### VM配置（逐一登录4个VM进行相同的操作）

* 修改etc/hostname：把VM-X-X-X改成VM自己的名字
* sudo reboot重启, 稍等片刻后再登录
* 增加user cse
```
sudo adduser cse
```
* 使cse有免密sudo权限，这一步小心操作
```c
sudo visudo
//在文件末尾增加cse ALL=(ALL:ALL) NOPASSWD: ALL 
```
* （sudo）每个VM的etc/hosts中增加四个VM的 private ip -- hostname，并删除127.0.0.1后的hostname

* 各自运行ssh-keygen，将id_rsa.pub(自己的公钥）放到每个VM（包括自己）cse用户的.ssh/authorized_keys中

* 将rododo的公钥加到app的auth中

* 在四个VM上配置Hadoop

* 在lab目录下建立app_public_ip文件，写入app的public ip

* docker ssh cse@app 成功，通过 part0 和 part1




