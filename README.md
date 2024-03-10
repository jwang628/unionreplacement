# unionreplacement
Postgresql extension Union Replacement of query OR logics

A very simple example:<br>
SELECT t1.id FROM table1 t1 INNER JOIN table2 t2 ON (t1.id=t2.id) WHERE t1.indexed_col1='value1' OR t2.indexed_col2='value2';

PG nor MRDB would use indexes and the only option to improve the query performance is replacing OR logic with UNION.

This extension achive this from PG server side if the query is hinted:<br>
SELECT t1.id FROM table1 t1 INNER JOIN table2 t2 ON (t1.id=t2.id) WHERE t1.indexed_col1='value1' OR t2.indexed_col2='value2' /*urhint: _1 */;

<b>Installation</b>:<br>
make && sudo make install<br>
add the extension:<br>
shared_preload_libraries = 'your_existing_extensions, unionreplacement'<br>
restart postgresql<br>
<br>
<b>sample hints</b> ( format: special_begin_numberofors_terminator )<br><pre>
        WHERE cond1 AND (cond2 OR (cond3 OR (cond4 OR cond5))); hint: nesteddoll_and_3
        WHERE (cond1 OR cond2 OR cond3) AND cond4;              hint: _(_2_and
        WHERE (cond1 OR cond2);                                 hint: _(_1
        WHERE cond1 AND (cond2 OR cond2 OR cond3);              hint: \_and_3\_)
        WHERE cond1 OR cond2;                                   hint: _where_1
        WHERE cond1 AND (cond2 OR cond3) ORDER BY;              hint: _and_1_order
        WHERE cond1 AND (cond2 OR cond3 OR cond4);              hint: _and_2
        WHERE cond1 AND ((cond2) OR (cond3) OR (cond4)) ORDER ; hint: bracked_and_2_order
        WHERE cond1 AND ((cond2) OR (cond3));                   hint: bracked_and_2
        WHERE ((cond1) OR (cond2));                             hint: bracked_(_1
        WHERE cond1 AND (cond2 OR cond3) AND cond4;             hint: _and_1_and
</pre>
