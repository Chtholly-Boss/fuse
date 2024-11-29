#let infos = (
  开课学期: "2024 秋季学期",
  课程名称: "操作系统",
  实验名称: "基于FUSE的青春版EXT2文件系统",
  学生班级: "9班",
  学生学号: "220110915",
  学生姓名: "马奕斌",
  评阅教师: "",
  报告成绩: "",
)

#let cover = [
  #grid(
    columns: (4fr, 1fr),
    rows: (10%, ), 
    align: horizon,
    image("./assets/hit.png"), 
    text(size: 30pt, [(深圳)])
  )

  #block(height: 5%)

  #align(
    center,
    block(
      height: 10%
    )[
      #text(
        size: 55pt,
        weight: "extrabold",
        font: "SimHei"
      )[实验报告])
    ]
  )

  #block(height: 5%)

  #align(center,)[
    #text(size: 18pt)[
      #table(
        columns: (2),
        stroke: none, 
        inset: 10pt,
        ..for (key, value) in infos.pairs() {
          (
            text(font: "STZhongsong", weight: "bold", [#key: ]),
            value,
            table.hline(start: 1,))
        }
      )
    ]
  ]

  #block(height: 5%)

  #align(center,)[
    #text(size: 18pt)[
      2024年11月
    ]
  ]
]

#let apply-template(body) = {
  cover
  pagebreak()

  set page(
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

  show raw: it => {
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

  show link: it => {
    set text(blue)
    underline(it)
  }

  // set quote(block: true)
  show quote: it => {
    set align(center)
    set text(size: 12pt, red)
    set block(fill: luma(95%), radius: 5pt, outset: 5pt, width: 100%)
    block(it.body)
  }

  
  import "@preview/numbly:0.1.0":numbly
  set heading(numbering: numbly(
    "{1:一、}", 
    "{2:1}", 
    "{2:1}.{3}",
    ""
  ))

  body
}


#import "@preview/gentle-clues:1.0.0" as clues
#let design_choice(content) = clues.idea(title: "Design Choice", content)

#let DS(name: none, description: none, funcs: none) = {
  set rect(width: 100%)
  grid(
    rows: 3, 
    align: left,
    rect(name),
    rect(description, height: 8%),
    rect(funcs, height: 15%)
  )
}
