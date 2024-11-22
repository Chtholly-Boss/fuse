#import "@preview/numbly:0.1.0":numbly

#set page(
  header: context [
    #set align(right)
    #set text(fill: luma(40%))
    《操作系统》实验报告-2024秋
    #line(length: 100%)
  ], 
  footer: context [
  #set align(right)
  #set text(size: 10pt)
    #line(length: 100%)
  #counter(page).display("1")
])

// Set the numbering style for headings. {level: format}
#set heading(numbering: numbly(
  "{1: 一、}", 
  "{2: 1. }"
))

#include "cover.typ"

#include "./part1-design.typ"
#include "./part2-problems.typ"
#include "./part3-suggestions.typ"
#include "./part4-refs.typ"
