#import "@preview/numbly:0.1.0":numbly
// 设置代码块样式
#show raw: it => {
  if it.block {
    block(
      fill: luma(95%),
      radius: 5pt,
      outset: 5pt, 
      width: 100%,
      )[#it]
  } else {
    it
  }
}

// 设置链接样式
#show link: underline

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
  "{1:一、}", 
  "{2:1}", 
  "{2:1}.{3}",
  ""
))

#include "cover.typ"

#include "./part1-design.typ"
#include "./part2-problems.typ"
#include "./part3-suggestions.typ"
#include "./part4-refs.typ"
