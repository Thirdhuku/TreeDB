TreeDB v1.0
======

a fast key-value storage engine based on Append-Only B+tree

### description
The typical key-value storage engine based on B+tree is the [Kyoto Cabinet TreeDB](http://fallabs.com/kyotocabinet/), but the Kyoto Cabinet TreeDB use hashDB to store its nodes, so the performance is so bad in the case of massive data, so I developed another treeDB to optimize storage performance.



### features
 - Batch-write to reduce I/O frequency
 - Append-Only to guarantee sequence write
 - Cache-Oblivous B+ tree to speed up

### Document
TODO


### Future
FastDB v2.0 use buffer-tree.
