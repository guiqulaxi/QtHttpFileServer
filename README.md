# QtHttpFileServer
基于Qt的http文件服务器,抄袭JQHttpServer中的网络，不推荐直接用。

POST 上传文件  localhost:2134/file/upload/dir1/dir2/dir3/filename 没有则创建目录
             
    200（OK）- 如果现有资源已被更改
    201（created）- 如果新资源被创建
    500 （internal server error）- 通用错误响应
    
GET  下载文件       localhost:2134/file/download/dir1/dir2/dir3/filename 没有则创建目录
     获得文件信息    localhost:2134/file/download/dir2/dir3/filename  json
     获得文件夹列表  localhost:2134/dir/list/dir1/dir2/dir3/  json
    
    200（OK） - 表示已在响应中发出
    404 （not found）- 资源不存在
    500 （internal server error）- 通用错误响应

DELETE 删除 localhost:2134/file/delete/dir1/dir2/dir3/filename
            localhost:2134/dir/delete/dir1/dir2/dir3/filename  
200 （OK）- 资源已被删除
404 （not found）- 资源不存在
500 （internal server error）- 通用错误响应
