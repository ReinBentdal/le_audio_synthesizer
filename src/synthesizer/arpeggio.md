## Criteria
- Structured in a way to easily support different modes of arpeggio (up, down, up and down, random, ..)
- Efficiency in mind

## Discussion
Two possibilities:
- Linked List: Particulary not appropriate for random access of elements
- Array, need to move allot of elements around. But shouldnt be any more inefficient than looping through a linked list. Suitable for random access.
  Harder to make the arp sound smooth while notes are added and removed

=> Array should be more suitable, especially since the size is not that large