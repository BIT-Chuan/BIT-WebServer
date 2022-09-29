create database studentDB;
use studentDB;
create table student
(
id int(10) primary key not null,
name char(4) not null,
class int(8) not null
);
insert into student values(111,'张三',07111905);
insert into student values(222,'李四',07111902);
insert into student values(333,'王五',07111907);
insert into student values(444,'赵六',07111903);
select * from studentDB.student;