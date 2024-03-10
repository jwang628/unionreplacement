# unionreplacement
Postgresql extension Union Replacement of query OR logics

A very simple example:  SELECT t1.id FROM table1 t1 INNER JOIN table2 t2 ON (t1.id=t2.id) WHERE t1.indexed_col1='value1' OR t2.indexed_col2='value2';

PG nor MRDB would use indexes and the only option to improve the query performance is replacing OR logic with UNION.

This extension achive this from PG server side if the query is hinted:
SELECT t1.id FROM table1 t1 INNER JOIN table2 t2 ON (t1.id=t2.id) WHERE t1.indexed_col1='value1' OR t2.indexed_col2='value2' /*urhint: _1 */;

Installation:

make && sudo make install

add the extension:

shared_preload_libraries = 'your_existing_extensions, unionreplacement'

restart postgresql
