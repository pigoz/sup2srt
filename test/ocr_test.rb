# frozen_string_literal: true

require 'rubygems'
require 'bundler/setup'
Bundler.require(:default, :test)

require_relative '../src/ocr_jpn.rb'

describe 'ocr' do
  results = [
    "(黄前久美子) 喜んでもらえて\nホッとしたよ",
    "(塚本秀一) これ渡すために\n呼び 出したのか",
    'そつか',
    '(久美子) あ:…あ…レ',
    '(足音)',
    'うん なんか他に用事あるの?',
    "みんな 美玲ちゃんの気持ちは\n分かってると思うから"
  ]

  files = Dir[File.expand_path('./ocr_fixtures/*.png', __dir__)]
  files.sort.each do |file|
    it "works for #{File.basename(file)}" do
      result = OcrJpn.new(file).call
      expect(result).to eql(results[File.basename(file, '.png').to_i - 1])
    end
  end
end
