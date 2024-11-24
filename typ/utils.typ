#let hint(content) = align(
  center, 
  quote(
    block: true,
    text(fill: red, content)))

#import "@preview/gentle-clues:1.0.0" as clues

#let design_choice(content) = clues.idea(title: "Design Choice", content)
