TreeDB
======

a fast key-value storage engine base on Append-Only B+tree

### description
The typical key-value storage engine base on B-tree is the [Kyoto Cabinet TreeDB](http://fallabs.com/kyotocabinet/), but the Kyoto Cabinet TreeDB use hashDB to store its nodes, so the performance is so bad in massive data case, so I developed another treeDB to optimize storage performance.



### feature
 - Batch-write to reduce I/O frequency
 - Append-Only to guarantee sequence write
 - Cache-Oblivous B+ tree to speed up
