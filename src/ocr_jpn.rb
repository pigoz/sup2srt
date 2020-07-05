# frozen_string_literal: true

class OcrJpn
  MAIN_TEXT_H = 52
  MAIN_TEXT_W = 52

  def initialize(image_path)
    @image_path = image_path
  end

  def call
    img = MiniMagick::Image.open(@image_path)
    vertical = img.height > img.width

    tesseract = RTesseract.new(
      @image_path,
      lang: vertical ? 'jpn_vert' : 'jpn+eng',
      psm: vertical ? 5 : 6,
      osm: 1
    )

    bbox = tesseract.to_box

    line_number = 0
    bbox = bbox.each_with_index.map do |x, i|
      prev = bbox[i - 1]
      x[:h] = x[:y_end] - x[:y_start]
      x[:w] = x[:x_end] - x[:x_start]
      axis = vertical ? 'y' : 'x'
      linebreak = i.positive? && x[:"#{axis}_start"] < prev[:"#{axis}_start"] *= 0.8
      line_number += 1 if linebreak
      x[:linebreak] = linebreak
      x[:line] = line_number
      x[:space] = i.positive? && (x[:"#{axis}_start"] > prev[:"#{axis}_end"] + MAIN_TEXT_W * 0.7)
      x
    end

    lines = bbox.group_by { |x| x[:line] }.values

    lines = lines.select do |line|
      hs = line.map { |x| x[:h] }
      avg = hs.sum / hs.size
      avg > MAIN_TEXT_H * 3 / 4
    end

    bbox = lines.flatten

    strings = bbox.map do |el|
      parts = [el[:space] ? ' ' : nil, el[:linebreak] ? "\n" : nil, el[:word]]
      parts.compact.join
    end

    fix(strings.join)
  end

  private

  def fix(text)
    text.gsub('芋前', '黄前').gsub(')', ') ').strip
  end
end
