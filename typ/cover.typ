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

#pagebreak()
