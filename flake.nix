{
  description = "Brumm Brumm Bau";

  inputs.nixpkgs.url = "nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }:
  let
    system = "aarch64-darwin";
    pkgs = import nixpkgs { inherit system; };

    vulkan-shaderc = pkgs.buildEnv {
      name = "vulkan-shaderc";
      paths = with pkgs; [
        vulkan-headers
        vulkan-loader
        vulkan-utility-libraries
        shaderc.dev
        shaderc.lib
      ];
      postBuild = ''
        ln -sf "${pkgs.shaderc.lib}/lib/libshaderc_shared.dylib" "$out/lib/libshaderc_combined.dylib"
      '';
    };
  in {
    devShells.${system}.default = pkgs.mkShell {
      packages = with pkgs; [
        cmake
        clang
        vulkan-headers
        vulkan-loader
        vulkan-utility-libraries
        shaderc
        moltenvk
      ];

      CMAKE_PREFIX_PATH = "${vulkan-shaderc}";
      DYLD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib:${pkgs.moltenvk}/lib";
      VK_ICD_FILENAMES = "${pkgs.moltenvk.out}/share/vulkan/icd.d/MoltenVK_icd.json";
    };
  };
}
