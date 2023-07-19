
## N-Airでプラグインを足す方法

### プラグインコンパイル

ここでは obs-plugintemplate に gain の音声フィルタだけを足したサンプルにしています。
id は gain_filter_x

ほぼそのままなので Visual C++ (communityなど) をインストールしてビルドすれば
build_x64/RelWithDebInfo/obs-plugintenplate.dll が作成されます。

これを obs なら Program Files/obs-studio/obs-plugins/64bit にコピーします。
起動し、音声のフィルタに GainX があれば成功です。

N-Air 開発では node_modules/obs-studio-node/obs-plugins/64bit にコピーします。
N-Air/SLOBS では許可したプラグインしか選択できないので、コード上でこれを許可します。

app/services/source-filter.ts の getTypesList whitelistedTypes に以下を足します。
(リスト外なのでとりあえずignore)

      //@ts-ignore
      { description: 'filters.gainx', value: 'gain_filter_x' },

起動し、音声のフィルタに filters.gainx があれば成功です。
